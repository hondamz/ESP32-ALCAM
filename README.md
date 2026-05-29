# ESP32-ALCAM

**Remote-Steuerung für Action-Cams via ESP32-S3**

[![Version](https://img.shields.io/badge/version-dev-blue)](https://github.com/hondamz/ESP32-ALCAM)
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

| Funktion | Beschreibung |
|----------|-------------|
| **Aufnahme Start** | Startet die Videoaufnahme |
| **Aufnahme Stop** | Stoppt die laufende Aufnahme |
| **Aufnahme Pause** | Pausiert die laufende Aufnahme |
| **Kamera Einschalten** | Weckt die Kamera aus dem Standby |
| **Kamera Ausschalten** | Schaltet die Kamera in den Standby |

---

## Kamera-Kopplung (Pairing)

Das Setup-Menü ermöglicht die Ersteinrichtung und Kopplung mit einer Action-Cam:

1. Setup-Menü aufrufen
2. Kamera in Pairing-Modus versetzen
3. ESP32-ALCAM sucht und verbindet sich mit der Kamera
4. Verbindungsdaten werden dauerhaft gespeichert (NVS)

---

## Hardware

- **Mikrocontroller:** ESP32-S3
- **Framework:** Arduino IDE
- **Kommunikation:** Bluetooth LE / WiFi

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
| dev | 2026-05-29 | Projektstart, Grundstruktur angelegt |

---

## Lizenz

MIT License – siehe [LICENSE](LICENSE)
