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
- Die **32 Eingänge + 32 Ausgänge** belegen den Mega fast vollständig (siehe Tabelle unten). Für Reserve / saubere Verdrahtung optional **I/O-Expander** (MCP23017 über I²C) – die Klassenlogik bleibt gleich.
- Geplant: **DS3231 RTC** (Uhrzeit-Logik), **Watchdog**, Zustandssicherung nach Netz-Ein.
- **Eingangs-Polarität** (`activeLow`) an die Vorschalt-Relais anpassen (nie > 5 V auf einen Pin).

## E/A-Belegung (Standard-Pinbelegung im Sketch)

Pins wie in `LichtsteuerungArduino.ino` (`IN[]` / `OUT[]` / `PIN_HELL`), an die reale
Verdrahtung anpassbar. **Reserviert:** `D0/D1` = Serial (Debug), `D2` = Helligkeitsimpuls
(INT0), `D13` = Onboard-LED (frei), `D20/D21` = I²C (SDA/SCL) für RTC.

Die Spalte *Funktion* nennt die Bezeichnung laut SPS-Belegung. Welche **Taste welche Lampe**
schaltet (inkl. Sonderfälle wie Gang, Küche, Aussenlampe) steht unter **Funktionsumfang** bzw. im Code.

### Eingänge (Taster / Sensoren)

| Nr | Pin | Funktion |
|---|---|---|
| S1  | D4  | Lampe Zimmer Mitte OG |
| S2  | D5  | Steckdose Zimmer Süd OG |
| S3  | D6  | Lampe Zimmer Südost OG |
| S4  | D7  | Lampe Zimmer Südwest OG |
| S5  | D8  | Steckdose Zimmer Südwest OG |
| S6  | D9  | Steckdose Zimmer Mitte OG |
| S7  | D10 | Store auf – Zimmer Südwest OG |
| S8  | D11 | Store ab – Zimmer Südwest OG |
| S9  | D12 | Steckdose Zimmer Nordost OG |
| S10 | D14 | Lampe Zimmer Nordost OG |
| S11 | D15 | PIR Bewegungsmelder, Gang OG |
| S12 | D16 | Ventilator Bad OG |
| S13 | D17 | Lampen Douche, Bad OG |
| S14 | D18 | Lampen Spiegel Bad OG |
| S15 | D19 | Taster Gang EG (auch „Küche-Insel aus") |
| S16 | A0  | Steckdose Zimmer Nordost EG |
| S17 | A1  | Lampe Zimmer Nordost EG |
| S18 | A2  | Steckdose Zimmer Mitteost EG |
| S19 | A3  | Lampe Zimmer Mitteost EG |
| S20 | A4  | Aussenlampe Süd (manuell) |
| S21 | A5  | Lampe oberhalb Ofen Wohnzimmer EG |
| S22 | A6  | Steckdose Wohnzimmer |
| S23 | A7  | Lampe Vorraum Nordwest EG |
| S24 | A8  | Taster Gang EG |
| S25 | A9  | Ventilator WC EG |
| S26 | A10 | Lampe WC EG |
| S27 | A11 | Lampe Küche oberhalb Insel EG |
| S28 | A12 | Lampe Küche oberhalb Insel EG |
| S29 | A13 | Taster Gang EG |
| S30 | A14 | Lampe Küche oberhalb Esstisch EG |
| S31 | A15 | Sensor Scheibenheizung Bad (kalt/feucht) |
| S32 | D2 (INT0) | Helligkeitssensor (Impuls) |

### Ausgänge (Relais)

| Nr | Pin | Funktion |
|---|---|---|
| L1  | D22 | Lampe Zimmer Mitte OG |
| L2  | D23 | Steckdose Zimmer Süd OG |
| L3  | D24 | Lampe Zimmer Südost OG |
| L4  | D25 | Lampe Zimmer Südwest OG |
| L5  | D26 | Steckdose Zimmer Südwest OG |
| L6  | D27 | Steckdose Zimmer Mitte OG |
| L7  | D28 | Rollladen *Fahren* – Südwest OG |
| L8  | D29 | Rollladen *Richtung* (auf/ab) |
| L9  | D30 | Steckdose Zimmer Nordost OG |
| L10 | D31 | Lampe Zimmer Nordost OG |
| L11 | D32 | Lampen Gang OG (Zusatzlampen) |
| L12 | D33 | Ventilator Bad OG |
| L13 | D34 | Lampen Douche, Bad OG |
| L14 | D35 | Lampen Spiegel Bad OG |
| L15 | D36 | Notlicht Gang OG |
| L16 | D37 | Steckdose Zimmer Nordost EG |
| L17 | D38 | Lampe Zimmer Nordost EG |
| L18 | D39 | Steckdose Zimmer Mitteost EG |
| L19 | D40 | Lampe Zimmer Mitteost EG |
| L20 | D41 | (frei / Reserve) |
| L21 | D42 | Aussenlampe Süd |
| L22 | D43 | Steckdose Wohnzimmer |
| L23 | D44 | Lampe oberhalb Ofen Wohnzimmer EG |
| L24 | D45 | Lampe Vorraum Nordwest EG |
| L25 | D46 | Lampe Gang EG |
| L26 | D47 | Notlicht Gang EG |
| L27 | D48 | Ventilator WC EG |
| L28 | D49 | Lampe WC EG |
| L29 | D50 | Lampe Küche oberhalb Insel EG |
| L30 | D51 | Lampe Küche/Gang EG |
| L31 | D52 | Lampe Küche oberhalb Esstisch EG |
| L32 | D53 | Scheibenheizung Bad OG |

## Build

```
arduino-cli compile --fqbn arduino:avr:mega --warnings all LichtsteuerungArduino
```
CI baut den Sketch automatisch bei jedem Push/PR (siehe Badge oben).

## Funktionsumfang

Die komplette SPS-Logik ist portiert:
- Alle Verbraucher (Lampen/Steckdosen/Ventilatoren) mit Taster- bzw. Schalter-Verhalten und Max-/Nachlaufzeiten
- Zentralfunktionen Hauptschalter / „Alle Lampen" (Langdruck)
- Gang OG: PIR + Helligkeit + Notlicht; Gang EG: 3 Taster + Notlicht + Küche/Gang-Kopplung
- Küche Insel, vom Gang (S15) ausschaltbar
- Rollladen (Fahren + Richtung, Fahrzeiten)
- Scheibenheizung Bad + Ventilator (gekoppelt)
- Aussenlampe dämmerungs-/zeitabhängig + manuell
- Helligkeitsmessung per Impulszählung

## Noch offen (Hardware/Betrieb)

- **RTC (DS3231)** anbinden und `uhr.setze(...)` im Setup füllen — ohne RTC sind die
  Zeitfenster inaktiv (nur Handbetrieb).
- **Watchdog** aktivieren, Zustandsverhalten nach Netz-Ein festlegen.
- **Pinbelegung** an die reale Verdrahtung anpassen; ggf. MCP23017-Expander.
- Auf realer Hardware testen (hier nur Kompilier-Check).
