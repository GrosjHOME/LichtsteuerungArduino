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

## Hardware-Konzept

- **Arduino Mega 2560**, Anbindung ans Hausnetz über **Ethernet-Shield** (SPI, W5500 empfohlen) für Remote-Funktionen.
- **Ausgänge (32 Relais):** 4× **8-Kanal-Relaismodul** (5 V, Stiftleiste) **direkt an native Arduino-Pins**. Diese Module sind meist **active-LOW** (IN = LOW → Relais an) → der Sketch invertiert die Ausgänge. Relaisspulen über **separate 5-V-Versorgung** (JD-VCC), nicht aus dem Arduino-Regler.
- **Eingänge (32 Kanäle):** 1× DIN-Board mit **2× MCP23017** (I²C, Schraubklemmen) — passt zu den Interface-Relais mit Schraubklemmen und belegt am Arduino nur **SDA/SCL (D20/D21)**.
- **Helligkeitsimpuls (S32):** nativer **Interrupt-Pin (D2)** — nicht über den Expander.
- **Reserviert fürs Ethernet-Shield (SPI):** D50–D53, D10 (CS), D4 (SD).
- Geplant: **DS3231 RTC** (Uhrzeit, am selben I²C-Bus), **Watchdog**, Zustandssicherung nach Netz-Ein.
- **Eingangs-Polarität** (`activeLow`) an die Interface-Relais anpassen (nie > 5 V auf einen Kanal).

## E/A-Belegung

**Ausgänge** = native Arduino-Pins (Relaismodule). **Eingänge** = Schraubklemmen des
MCP23017-Boards (`ICH`/`ICL`, Ports A/B). `S32` (Helligkeit) liegt am nativen Pin `D2`.
Die Spalte *Funktion* nennt die Bezeichnung laut SPS-Belegung; welche **Taste welche Lampe**
schaltet (Gang, Küche, Aussenlampe …) steht unter **Funktionsumfang** bzw. im Code.

> Hinweis: Der Sketch liest die Eingänge derzeit noch über native Pins (Bench-Test).
> Die Umstellung auf den MCP23017 (I²C) ist der nächste Code-Schritt.

### Eingänge (MCP23017-Board, Schraubklemmen)

| Nr | Klemme | Funktion |
|---|---|---|
| S1  | HA0 | Lampe Zimmer Mitte OG |
| S2  | HA1 | Steckdose Zimmer Süd OG |
| S3  | HA2 | Lampe Zimmer Südost OG |
| S4  | HA3 | Lampe Zimmer Südwest OG |
| S5  | HA4 | Steckdose Zimmer Südwest OG |
| S6  | HA5 | Steckdose Zimmer Mitte OG |
| S7  | HA6 | Store auf – Zimmer Südwest OG |
| S8  | HA7 | Store ab – Zimmer Südwest OG |
| S9  | HB0 | Steckdose Zimmer Nordost OG |
| S10 | HB1 | Lampe Zimmer Nordost OG |
| S11 | HB2 | PIR Bewegungsmelder, Gang OG |
| S12 | HB3 | Ventilator Bad OG |
| S13 | HB4 | Lampen Douche, Bad OG |
| S14 | HB5 | Lampen Spiegel Bad OG |
| S15 | HB6 | Taster Gang EG (auch „Küche-Insel aus") |
| S16 | HB7 | Steckdose Zimmer Nordost EG |
| S17 | LA0 | Lampe Zimmer Nordost EG |
| S18 | LA1 | Steckdose Zimmer Mitteost EG |
| S19 | LA2 | Lampe Zimmer Mitteost EG |
| S20 | LA3 | Aussenlampe Süd (manuell) |
| S21 | LA4 | Lampe oberhalb Ofen Wohnzimmer EG |
| S22 | LA5 | Steckdose Wohnzimmer |
| S23 | LA6 | Lampe Vorraum Nordwest EG |
| S24 | LA7 | Taster Gang EG |
| S25 | LB0 | Ventilator WC EG |
| S26 | LB1 | Lampe WC EG |
| S27 | LB2 | Lampe Küche oberhalb Insel EG |
| S28 | LB3 | Lampe Küche oberhalb Insel EG |
| S29 | LB4 | Taster Gang EG |
| S30 | LB5 | Lampe Küche oberhalb Esstisch EG |
| S31 | LB6 | Sensor Scheibenheizung Bad (kalt/feucht) |
| S32 | D2 (nativ, INT0) | Helligkeitssensor (Impuls) |

*(Klemme `LB7` frei.)*

### Ausgänge (Relaismodule, native Arduino-Pins, active-LOW)

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
