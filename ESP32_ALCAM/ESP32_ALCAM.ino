// ================================================================
//  ESP32-ALCAM  –  Remote Controller für Action-Cams
//  Board  : ESP32-S3
//  Version: 0.1.0
// ================================================================
//  Unterstützte Kameras:
//    - Insta360 X4          (aktiv)
//    - Insta360 X3          (geplant)
//    - GoPro 12             (geplant)
//
//  Steuerungsfunktionen:
//    - Aufnahme Start / Stop / Pause
//    - Kamera Ein / Ausschalten
//    - Kamera-Kopplung via Setup-Menü (gespeichert in NVS)
// ================================================================

#define APP_VERSION "0.1.0"

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEClient.h>
#include <BLERemoteService.h>
#include <BLERemoteCharacteristic.h>
#include <Preferences.h>

// ----------------------------------------------------------------
//  Insta360 X4 – BLE UUIDs (ermittelt per Service-Scan)
// ----------------------------------------------------------------
#define INSTA360_SERVICE_UUID        "0000be80-0000-1000-8000-00805f9b34fb"
#define INSTA360_WRITE_CHAR_UUID     "0000be81-0000-1000-8000-00805f9b34fb"  // READ+WRITE
#define INSTA360_NOTIFY_CHAR_UUID    "0000be82-0000-1000-8000-00805f9b34fb"  // NOTIFY
#define INSTA360_READ_CHAR_UUID      "0000be83-0000-1000-8000-00805f9b34fb"  // READ

// ----------------------------------------------------------------
//  BLE-Kommandos für Insta360 X4
// ----------------------------------------------------------------
// Protokoll: [Länge_Payload][Cmd-Gruppe][Cmd-ID][Params...]
// Länge = Anzahl der folgenden Bytes (ohne Länge-Byte selbst)
// Quelle: Insta360 Open SDK / Community Reverse Engineering
const uint8_t CMD_REC_START[]  = { 0x05, 0x02, 0x01, 0x00, 0x00, 0x00 };  // Aufnahme starten
const uint8_t CMD_REC_STOP[]   = { 0x05, 0x02, 0x02, 0x00, 0x00, 0x00 };  // Aufnahme stoppen
const uint8_t CMD_REC_PAUSE[]  = { 0x05, 0x02, 0x03, 0x00, 0x00, 0x00 };  // Aufnahme pausieren
const uint8_t CMD_CAM_OFF[]    = { 0x03, 0x05, 0x01, 0x00 };               // Kamera ausschalten
const uint8_t CMD_KEEP_ALIVE[] = { 0x01, 0x00 };                            // Keep-Alive / Heartbeat

// ----------------------------------------------------------------
//  Serielle Befehle (USB-Monitor)
// ----------------------------------------------------------------
#define CMD_SERIAL_START   "START"
#define CMD_SERIAL_STOP    "STOP"
#define CMD_SERIAL_PAUSE   "PAUSE"
#define CMD_SERIAL_OFF     "OFF"
#define CMD_SERIAL_SETUP   "SETUP"
#define CMD_SERIAL_STATUS  "STATUS"
#define CMD_SERIAL_HELP    "HELP"
#define CMD_SERIAL_SCAN    "SCAN"
#define CMD_SERIAL_RAW     "RAW"   // RAW:01 02 03 ...  – Hex-Bytes direkt senden
#define CMD_SERIAL_READ    "READ"  // be83 Characteristic lesen

// ----------------------------------------------------------------
//  Konfiguration
// ----------------------------------------------------------------
#define BLE_SCAN_DURATION_SEC   10
#define KEEP_ALIVE_INTERVAL_MS  30000
#define RECONNECT_INTERVAL_MS   10000
#define SERIAL_BAUD             115200

// ----------------------------------------------------------------
//  Globale Objekte
// ----------------------------------------------------------------
Preferences prefs;

BLEClient*              pClient       = nullptr;
BLERemoteCharacteristic* pWriteChar   = nullptr;
BLERemoteCharacteristic* pNotifyChar  = nullptr;
BLEScan*                pBLEScan      = nullptr;

// ----------------------------------------------------------------
//  Zustandsvariablen
// ----------------------------------------------------------------
bool        connected        = false;
bool        recording        = false;
bool        paused           = false;
String      savedDeviceName  = "";
String      savedDeviceAddr  = "";
uint32_t    lastKeepAlive    = 0;
uint32_t    lastReconnect    = 0;

// ================================================================
//  NVS – Kamera-Adresse laden / speichern
// ================================================================
void loadCameraSettings() {
  prefs.begin("alcam", true);
  savedDeviceName = prefs.getString("cam_name", "");
  savedDeviceAddr = prefs.getString("cam_addr", "");
  prefs.end();
}

void saveCameraSettings(const String& name, const String& addr) {
  prefs.begin("alcam", false);
  prefs.putString("cam_name", name);
  prefs.putString("cam_addr", addr);
  prefs.end();
  savedDeviceName = name;
  savedDeviceAddr = addr;
  Serial.println("[NVS] Kamera gespeichert: " + name + " (" + addr + ")");
}

void clearCameraSettings() {
  prefs.begin("alcam", false);
  prefs.clear();
  prefs.end();
  savedDeviceName = "";
  savedDeviceAddr = "";
  Serial.println("[NVS] Kamera-Kopplung gelöscht.");
}

// ================================================================
//  BLE – Benachrichtigungs-Callback
// ================================================================
// Letztes Notify-Paket zum Entfiltern von Duplikaten
static uint8_t lastNotify[32];
static size_t  lastNotifyLen = 0;

void notifyCallback(BLERemoteCharacteristic* pChar, uint8_t* pData, size_t length, bool isNotify) {
  // Bekannte Heartbeat-Muster stumm schalten (05 00 00 / 07 00 00 00 ...)
  bool isHeartbeat = (length <= 7 &&
    (pData[0] == 0x05 || pData[0] == 0x07 || pData[0] == 0x00) &&
    (length < 2 || pData[1] == 0x00 || pData[1] == 0x05));
  if (isHeartbeat) return;

  // Identische Pakete nicht wiederholen
  if (length == lastNotifyLen && memcmp(pData, lastNotify, length) == 0) return;
  memcpy(lastNotify, pData, min(length, (size_t)32));
  lastNotifyLen = length;

  Serial.print("[CAM->ESP] ");
  for (size_t i = 0; i < length; i++) Serial.printf("%02X ", pData[i]);
  Serial.print("| ");
  for (size_t i = 0; i < length; i++)
    Serial.print(pData[i] >= 0x20 && pData[i] < 0x7F ? (char)pData[i] : '.');
  Serial.println();
}

// ================================================================
//  BLE – Verbindungsstatus-Callbacks
// ================================================================
class ClientCallbacks : public BLEClientCallbacks {
  void onConnect(BLEClient* pclient) override {
    Serial.println("[BLE] Verbunden.");
  }
  void onDisconnect(BLEClient* pclient) override {
    connected = false;
    recording = false;
    paused    = false;
    Serial.println("[BLE] Verbindung getrennt – versuche Reconnect...");
  }
};

// ================================================================
//  BLE – Scan-Callbacks (für Setup/Pairing)
// ================================================================
class AdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
public:
  String foundName;
  String foundAddr;
  bool   found = false;

  void onResult(BLEAdvertisedDevice advertisedDevice) override {
    String name = advertisedDevice.getName().c_str();
    String addr = advertisedDevice.getAddress().toString().c_str();
    Serial.println("  Gefunden: " + name + "  [" + addr + "]");

    // Insta360 Geräte erkennen (Name beginnt mit "Insta360")
    if (name.startsWith("Insta360") || name.startsWith("X4") || name.startsWith("X3")) {
      foundName = name;
      foundAddr = addr;
      found     = true;
      advertisedDevice.getScan()->stop();
    }
  }
};

// ================================================================
//  BLE – Alle Services und Characteristics ausgeben (Diagnose)
// ================================================================
void dumpBLEServices() {
  Serial.println("\n[DIAG] === BLE Service-Scan ===");
  std::map<std::string, BLERemoteService*>* services = pClient->getServices();
  if (services->empty()) {
    Serial.println("[DIAG] Keine Services gefunden.");
    return;
  }
  for (auto& svc : *services) {
    Serial.println("[DIAG] Service: " + String(svc.first.c_str()));
    std::map<std::string, BLERemoteCharacteristic*>* chars = svc.second->getCharacteristics();
    for (auto& ch : *chars) {
      String props = "";
      BLERemoteCharacteristic* c = ch.second;
      if (c->canRead())    props += "READ ";
      if (c->canWrite())   props += "WRITE ";
      if (c->canNotify())  props += "NOTIFY ";
      if (c->canIndicate()) props += "INDICATE ";
      Serial.println("  Char: " + String(ch.first.c_str()) + "  [" + props + "]");
    }
  }
  Serial.println("[DIAG] === Ende Service-Scan ===\n");
}

// ================================================================
//  BLE – Verbindung aufbauen
// ================================================================
bool connectToCamera(const String& address) {
  Serial.println("[BLE] Verbinde mit " + address + " ...");

  if (pClient != nullptr) {
    pClient->disconnect();
    delete pClient;
  }

  pClient = BLEDevice::createClient();
  pClient->setClientCallbacks(new ClientCallbacks());

  BLEAddress bleAddr(address.c_str());
  if (!pClient->connect(bleAddr)) {
    Serial.println("[BLE] Verbindung fehlgeschlagen.");
    return false;
  }

  // Dienst suchen
  BLERemoteService* pService = pClient->getService(INSTA360_SERVICE_UUID);
  if (pService == nullptr) {
    Serial.println("[BLE] Insta360-Dienst nicht gefunden.");
    pClient->disconnect();
    return false;
  }

  // Schreib-Characteristic
  pWriteChar = pService->getCharacteristic(INSTA360_WRITE_CHAR_UUID);
  if (pWriteChar == nullptr) {
    Serial.println("[BLE] Write-Characteristic nicht gefunden.");
    pClient->disconnect();
    return false;
  }

  // Notify-Characteristic (optional)
  pNotifyChar = pService->getCharacteristic(INSTA360_NOTIFY_CHAR_UUID);
  if (pNotifyChar != nullptr && pNotifyChar->canNotify()) {
    pNotifyChar->registerForNotify(notifyCallback);
  }

  connected = true;
  Serial.println("[BLE] ✓ Verbunden mit " + savedDeviceName);
  return true;
}

// ================================================================
//  BLE – Kommando senden
// ================================================================
bool sendCommand(const uint8_t* cmd, size_t len) {
  if (!connected || pWriteChar == nullptr) {
    Serial.println("[ERR] Nicht verbunden.");
    return false;
  }
  pWriteChar->writeValue(const_cast<uint8_t*>(cmd), len);
  return true;
}

// ================================================================
//  Kamera-Steuerungsfunktionen
// ================================================================
void camRecordStart() {
  if (recording && !paused) { Serial.println("[INFO] Aufnahme läuft bereits."); return; }
  if (sendCommand(CMD_REC_START, sizeof(CMD_REC_START))) {
    recording = true;
    paused    = false;
    Serial.println("[CAM] ▶ Aufnahme gestartet.");
  }
}

void camRecordStop() {
  if (!recording) { Serial.println("[INFO] Keine aktive Aufnahme."); return; }
  if (sendCommand(CMD_REC_STOP, sizeof(CMD_REC_STOP))) {
    recording = false;
    paused    = false;
    Serial.println("[CAM] ■ Aufnahme gestoppt.");
  }
}

void camRecordPause() {
  if (!recording) { Serial.println("[INFO] Keine aktive Aufnahme zum Pausieren."); return; }
  if (sendCommand(CMD_REC_PAUSE, sizeof(CMD_REC_PAUSE))) {
    paused = !paused;
    Serial.println(paused ? "[CAM] ⏸ Aufnahme pausiert." : "[CAM] ▶ Aufnahme fortgesetzt.");
  }
}

void camPowerOff() {
  if (sendCommand(CMD_CAM_OFF, sizeof(CMD_CAM_OFF))) {
    Serial.println("[CAM] ⏻ Kamera ausschalten...");
    delay(500);
    pClient->disconnect();
  }
}

// ================================================================
//  Setup-Modus: Kamera suchen und koppeln
// ================================================================
void runSetupMode() {
  Serial.println("\n=== SETUP-MODUS ===");
  Serial.println("Starte BLE-Scan nach Insta360-Kameras...");
  Serial.println("(Kamera muss eingeschaltet und Bluetooth aktiv sein)\n");

  AdvertisedDeviceCallbacks* cb = new AdvertisedDeviceCallbacks();
  pBLEScan->setAdvertisedDeviceCallbacks(cb);
  pBLEScan->setActiveScan(true);
  pBLEScan->start(BLE_SCAN_DURATION_SEC, false);

  if (cb->found) {
    Serial.println("\nKamera gefunden: " + cb->foundName);
    Serial.println("Adresse: " + cb->foundAddr);
    Serial.println("Versuche Verbindung...");

    if (connectToCamera(cb->foundAddr)) {
      saveCameraSettings(cb->foundName, cb->foundAddr);
      Serial.println("✓ Kamera erfolgreich gekoppelt und gespeichert!");
    } else {
      Serial.println("✗ Verbindung fehlgeschlagen. Setup wiederholen.");
    }
  } else {
    Serial.println("✗ Keine Insta360-Kamera gefunden.");
    Serial.println("  Tipp: Kamera einschalten, Bluetooth aktivieren, dann SETUP erneut eingeben.");
  }

  pBLEScan->clearResults();
  delete cb;
}

// ================================================================
//  Status-Ausgabe
// ================================================================
void printStatus() {
  Serial.println("\n--- STATUS ---");
  Serial.println("Version     : " APP_VERSION);
  Serial.println("Kamera      : " + (savedDeviceName.length() ? savedDeviceName : "keine gekoppelt"));
  Serial.println("Adresse     : " + (savedDeviceAddr.length() ? savedDeviceAddr : "-"));
  Serial.println("Verbunden   : " + String(connected ? "JA" : "NEIN"));
  Serial.println("Aufnahme    : " + String(recording ? (paused ? "PAUSIERT" : "LÄUFT") : "GESTOPPT"));
  Serial.println("--------------\n");
}

void printHelp() {
  Serial.println("\n=== BEFEHLE (Serieller Monitor) ===");
  Serial.println("  START   – Aufnahme starten");
  Serial.println("  STOP    – Aufnahme stoppen");
  Serial.println("  PAUSE   – Aufnahme pausieren/fortsetzen");
  Serial.println("  OFF     – Kamera ausschalten");
  Serial.println("  SETUP   – Kamera suchen und koppeln");
  Serial.println("  STATUS  – Aktuellen Status anzeigen");
  Serial.println("  HELP    – Diese Hilfe anzeigen");
  Serial.println("  SCAN    – BLE Services auflisten (verbunden)");
  Serial.println("  READ    – Status-Characteristic be83 lesen");
  Serial.println("  RAW:XX XX XX ... – Rohe Hex-Bytes senden");
  Serial.println("  Beispiel: RAW:05 02 01 00 00 00");
  Serial.println("=====================================\n");
}

// ================================================================
//  RAW-Hex-Bytes senden  (z.B. "RAW:05 02 01 00 00 00")
// ================================================================
void sendRawHex(const String& hexStr) {
  if (!connected || pWriteChar == nullptr) { Serial.println("[ERR] Nicht verbunden."); return; }
  uint8_t buf[32];
  int len = 0;
  String s = hexStr;
  s.trim();
  int pos = 0;
  while (pos < (int)s.length() && len < 32) {
    while (pos < (int)s.length() && s[pos] == ' ') pos++;
    if (pos + 1 >= (int)s.length()) break;
    String byteStr = s.substring(pos, pos + 2);
    buf[len++] = (uint8_t)strtol(byteStr.c_str(), nullptr, 16);
    pos += 2;
  }
  if (len == 0) { Serial.println("[ERR] Keine gültigen Hex-Bytes."); return; }
  Serial.print("[ESP->CAM] ");
  for (int i = 0; i < len; i++) Serial.printf("%02X ", buf[i]);
  Serial.println();
  pWriteChar->writeValue(buf, len);
}

// ================================================================
//  be83 READ-Characteristic auslesen
// ================================================================
void readStatusChar() {
  if (!connected || pClient == nullptr) { Serial.println("[ERR] Nicht verbunden."); return; }
  BLERemoteService* svc = pClient->getService(INSTA360_SERVICE_UUID);
  if (!svc) return;
  BLERemoteCharacteristic* rc = svc->getCharacteristic(INSTA360_READ_CHAR_UUID);
  if (!rc || !rc->canRead()) { Serial.println("[ERR] Read-Char nicht verfügbar."); return; }
  String val = rc->readValue();
  Serial.print("[be83 READ] ");
  for (size_t i = 0; i < (size_t)val.length(); i++) Serial.printf("%02X ", (uint8_t)val[i]);
  Serial.println();
}

// ================================================================
//  Serielle Eingabe verarbeiten
// ================================================================
void handleSerialInput() {
  if (!Serial.available()) return;
  String input = Serial.readStringUntil('\n');
  input.trim();
  input.toUpperCase();

  if      (input == CMD_SERIAL_START)  camRecordStart();
  else if (input == CMD_SERIAL_STOP)   camRecordStop();
  else if (input == CMD_SERIAL_PAUSE)  camRecordPause();
  else if (input == CMD_SERIAL_OFF)    camPowerOff();
  else if (input == CMD_SERIAL_SETUP)  runSetupMode();
  else if (input == CMD_SERIAL_STATUS) printStatus();
  else if (input == CMD_SERIAL_HELP)   printHelp();
  else if (input == CMD_SERIAL_SCAN)              { if (connected) dumpBLEServices(); else Serial.println("[ERR] Nicht verbunden."); }
  else if (input == CMD_SERIAL_READ)              readStatusChar();
  else if (input.startsWith(CMD_SERIAL_RAW ":")) sendRawHex(input.substring(4));
  else if (input.length() > 0)                    Serial.println("[?] Unbekannter Befehl. HELP eingeben.");
}

// ================================================================
//  Keep-Alive & Auto-Reconnect
// ================================================================
void handleKeepAlive() {
  if (!connected) return;
  if (millis() - lastKeepAlive < KEEP_ALIVE_INTERVAL_MS) return;
  lastKeepAlive = millis();
  sendCommand(CMD_KEEP_ALIVE, sizeof(CMD_KEEP_ALIVE));
}

void handleAutoReconnect() {
  if (connected) return;
  if (savedDeviceAddr.length() == 0) return;
  if (millis() - lastReconnect < RECONNECT_INTERVAL_MS) return;
  lastReconnect = millis();
  Serial.println("[BLE] Auto-Reconnect zu " + savedDeviceName + "...");
  connectToCamera(savedDeviceAddr);
}

// ================================================================
//  Setup
// ================================================================
void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(1000);

  Serial.println("\n========================================");
  Serial.println("  ESP32-ALCAM  v" APP_VERSION);
  Serial.println("  Action-Cam Remote Controller");
  Serial.println("========================================\n");

  // NVS laden
  loadCameraSettings();

  // BLE initialisieren
  BLEDevice::init("ESP32-ALCAM");
  pBLEScan = BLEDevice::getScan();
  pBLEScan->setActiveScan(true);

  // Gespeicherte Kamera verbinden
  if (savedDeviceAddr.length() > 0) {
    Serial.println("[NVS] Bekannte Kamera: " + savedDeviceName + " (" + savedDeviceAddr + ")");
    Serial.println("[BLE] Verbinde automatisch...");
    if (!connectToCamera(savedDeviceAddr)) {
      Serial.println("[BLE] Automatische Verbindung fehlgeschlagen – warte auf Reconnect.");
    }
  } else {
    Serial.println("[INFO] Keine Kamera gekoppelt. SETUP eingeben zum Koppeln.");
  }

  printHelp();
}

// ================================================================
//  Loop
// ================================================================
void loop() {
  handleSerialInput();
  handleKeepAlive();
  handleAutoReconnect();
  delay(10);
}
