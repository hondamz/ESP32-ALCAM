# ESP32-ALCAM

**Remote-Steuerung für Action-Cams via ESP32-S3**

[![Version](https://img.shields.io/badge/version-0.1.0-blue)](https://github.com/hondamz/ESP32-ALCAM)
[![Platform](https://img.shields.io/badge/platform-ESP32--S3-green)](https://github.com/hondamz/ESP32-ALCAM)

---

## Funktionsbeschreibung

ESP32-ALCAM steuert eine Action-Kamera drahtlos per Bluetooth/WiFi über einen ESP32-S3. Das Programm verbindet sich mit der Kamera und ermöglicht die vollständige Fernsteuerung der wichtigsten Aufnahmefunktionen.

### Unterstützte Kameras

| Kamera | Status |
|--------|--------|
| Insta360 X4 | ✅ Unterstützt |
| Insta360 X3 | 🔜 Geplant |
| GoPro 12 | 🔜 Geplant |

---

## Steuerungsfunktionen

| Funktion | Serieller Befehl | Beschreibung |
|----------|-----------------|-------------|
| **Aufnahme Start** | `START` | Startet die Videoaufnahme |
| **Aufnahme Stop** | `STOP` | Stoppt die laufende Aufnahme |
| **Aufnahme Pause** | `PAUSE` | Pausiert / setzt Aufnahme fort |
| **Kamera Ausschalten** | `OFF` | Schaltet die Kamera aus |
| **Status anzeigen** | `STATUS` | Verbindungs- und Aufnahmestatus |
| **Hilfe** | `HELP` | Alle Befehle anzeigen |

---

## Kamera-Kopplung (Pairing)

Das Setup-Menü ermöglicht die Ersteinrichtung und Kopplung mit einer Action-Cam:

1. Setup-Menü aufrufen
2. Kamera in Pairing-Modus versetzen
3. ESP32-ALCAM sucht und verbindet sich mit der Kamera
4. Verbindungsdaten werden dauerhaft gespeichert (NVS)

---

## Verwendung (Serieller Monitor)

Nach dem Flashen den Seriellen Monitor mit **115200 Baud** öffnen:

1. Beim ersten Start: `SETUP` eingeben → ESP scannt nach Insta360-Kameras
2. Kamera wird gefunden, verbunden und Adresse in NVS gespeichert
3. Ab sofort verbindet sich der ESP automatisch beim Start
4. Steuerung per Befehle (siehe Tabelle oben)

---

## Hardware

- **Mikrocontroller:** ESP32-S3
- **Framework:** Arduino IDE
- **Protokoll:** Bluetooth LE (proprietäres Insta360-Protokoll)
- **Bibliotheken:** `BLEDevice`, `Preferences` (beide in ESP32 Arduino Core enthalten)

---

## Projektstruktur

```
ESP32-ALCAM/
├── ESP32_ALCAM/
│   └── ESP32_ALCAM.ino       ← Haupt-Sketch (Arduino)
├── README.md                  ← Funktionsbeschreibung (immer aktuell)
├── CLAUDE.md                  ← Projektanweisungen für Claude Code
├── git_push.sh                ← Push-Skript (nach Änderungen)
├── secrets.local              ← GitHub-Token (NICHT in Git)
└── .gitignore
```

---

## Changelog

| Version | Datum | Änderungen |
|---------|-------|-----------|
| 0.1.0 | 2026-05-29 | Projektstart: BLE-Grundgerüst, Insta360 X4, Serieller Monitor, NVS-Pairing |

---

## Lizenz

MIT License – siehe [LICENSE](LICENSE)
