# Projektanweisungen – ESP32-ALCAM

## GitHub-Workflow (PFLICHT nach jeder Codeänderung)

Nach **jeder** Änderung am Code oder der README musst du folgende Schritte ausführen:

### 1. README.md aktualisieren
- Versionsnummer anpassen (`APP_VERSION` aus dem Sketch übernehmen)
- Changelog-Zeile für die neue Version ergänzen (mit allen Änderungen)
- Unterstützte Kameras und Funktionen aktuell halten
- Betroffene Abschnitte aktualisieren

### 2. Commit und Push zu GitHub
```bash
cd /Users/andreasliedke/SynologyDriveShared/SynologyDrive/_entwicklung/ESP32-ALCAM
bash git_push.sh
```

Das Skript liest den Token aus `secrets.local`, setzt die Remote-URL, erstellt den Commit und pusht.

**Repository:** https://github.com/hondamz/ESP32-ALCAM

---

## Projektbeschreibung

ESP32-ALCAM steuert remote Action-Kameras (Insta360 X4, später X3 und GoPro 12) über einen ESP32-S3.

### Steuerungsfunktionen
- Aufnahme Start / Stop / Pause
- Kamera Ein- und Ausschalten
- Kamera-Kopplung über Setup-Menü (gespeichert in NVS via Preferences)

---

## Projektstruktur

```
ESP32-ALCAM/
├── ESP32_ALCAM/
│   └── ESP32_ALCAM.ino       ← Haupt-Sketch (Arduino)
├── README.md                  ← Funktionsbeschreibung (immer aktuell halten)
├── CLAUDE.md                  ← Diese Datei (Projektanweisungen)
├── git_push.sh                ← Push-Skript (nach Änderungen ausführen)
├── secrets.local              ← GitHub-Token (NICHT pushen, in .gitignore)
└── .gitignore
```

---

## Technische Rahmenbedingungen

- **Board:** ESP32-S3
- **Framework:** Arduino IDE
- **Kommunikation:** Bluetooth LE / WiFi
- **Konfiguration:** NVS via Preferences
- `secrets.local` enthält nur den GH_TOKEN – niemals committen

## Coding-Regeln

- Keine Abhängigkeiten von externen Cloud-Diensten – alles lokal
- `secrets.local` enthält nur den GH_TOKEN – niemals committen
