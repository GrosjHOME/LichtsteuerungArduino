# LichtsteuerungArduino

![Kompilier-Check](https://github.com/GrosjHOME/LichtsteuerungArduino/actions/workflows/kompilier-check.yml/badge.svg)

Arduino-Portierung der Haus-Lichtsteuerung (ursprünglich Siemens STEP 7 / CPU 614).
Ziel: die bewährte SPS-Logik auf einem **Arduino Mega 2560** nachbilden. Die 230-V-Trennung
erfolgt weiterhin **extern über Relais**.

## Aufbau

| Datei | Inhalt |
|---|---|
| `LichtsteuerungArduino.ino` | Hauptprogramm: Verbraucher-Objekte, Zentralfunktionen, Zyklus |
| `Wechselschaltung.h` | Wiederverwendbare Bausteine (Timer, Flanke, Entprellung, Kernlogik) |

## Bausteine

- **`Wechselschaltung`** – Kernbaustein je Verbraucher (entspricht dem STEP7-FB):
  - **Taster-Modus**: Tastendruck toggelt, Auto-Aus nach `maxZeit`, `AlleLampenEin`-Override, `Hauptschalter`-Freigabe.
  - **Schalter-Modus**: Ausgang folgt Eingang mit Nachlauf `asZeit`.
- **`LangdruckToggle`** – Zentralfunktionen (Hauptschalter / „Alle Lampen") per Langdruck, wie in OB1.
- **`Ton` / `Tof`** – Ein-/Ausschaltverzögerung, **millis()-rollover-sicher**.
- **`DebIn`** – entprellter Eingang (Polarität konfigurierbar).

## Hardware

- **Arduino Mega 2560**; Ausgänge auf die vorhandenen (extern entkoppelten) Relais.
- Für alle **32 Ein-/32 Ausgänge**: I/O-Expander (z. B. MCP23017 über I²C) – die Klassenlogik bleibt gleich.
- Geplant: **DS3231 RTC** (Uhrzeit-Logik), **Watchdog**, Zustandssicherung nach Netz-Ein.
- **Eingangs-Polarität** (`activeLow`) an die Vorschalt-Relais anpassen (nie > 5 V auf einen Pin).

## Build

```
arduino-cli compile --fqbn arduino:avr:mega --warnings all LichtsteuerungArduino
```
CI baut den Sketch automatisch bei jedem Push/PR (siehe Badge oben).

## Status / TODO

Grundgerüst (Kernlogik + Zentralfunktionen) steht. Noch nachzubilden:
Gang (PIR + Helligkeit + Notlicht), Rollladen (Fahren + Richtung), Aussenlampe
(dämmerungs-/zeitabhängig, RTC), Helligkeitszählung, Watchdog/RTC-Integration.
