// ============================================================================
//  Lichtsteuerung (Arduino Mega) - Portierung der STEP7-Logik nach C++
//
//  Struktur wie in der SPS:
//   - je Verbraucher ein "Wechselschaltung"-Objekt (Taster-Toggle ODER Schalter/Nachlauf)
//   - Zentralfunktionen (Hauptschalter / Alle Lampen) per Langdruck, wie in OB1
//
//  Diese Datei ist ein lauffaehiges Geruest mit einigen Beispiel-Verbrauchern.
//  Fuer alle 32 Ein-/32 Ausgaenge reichen die Mega-Pins knapp nicht sauber ->
//  I/O-Expander (z.B. MCP23017 ueber I2C) verwenden; die Klassen-Logik bleibt
//  identisch (nur digitalRead/Write gegen Expander-Zugriff tauschen).
//
//  NOCH ZU ERGAENZEN (siehe TODO unten): Gang(PIR+Helligkeit)+Notlicht,
//  Rollladen (Fahren+Richtung), Aussenlampe (Helligkeit+Uhrzeit/RTC),
//  Helligkeitszaehlung, Watchdog, Zustandssicherung.
// ============================================================================

#include "Wechselschaltung.h"

// --- Beispiel-Pinbelegung (Arduino Mega) ---
// Eingaenge: Polaritaet (activeLow) an die Vorschalt-Relais anpassen!
DebIn tS1(22), tS2(23), tS3(24), tS4(25), tS13(26), tS12(27);

// Verbraucher: (Ausgangspin, schalter?, asZeit[ms], maxZeit[ms])
Wechselschaltung L1 (30, false, 0,          3600000UL); // Lampe Zimmer Mitte OG - Taster, Max 1h
Wechselschaltung L2 (31, false, 0,          3600000UL); // Steckdose Sued OG      - Taster, 1h
Wechselschaltung L3 (32, false, 0,          3600000UL); // Lampe Suedost OG       - Taster, 1h
Wechselschaltung L4 (33, false, 0,          3600000UL); // Lampe Suedwest OG      - Taster, 1h
Wechselschaltung L13(34, false, 0,          7200000UL); // Lampen Douche Bad OG   - Taster, 2h
Wechselschaltung L12(35, true,  1800000UL,  0);         // Ventilator Bad OG      - Schalter, Nachlauf 30min

// --- Zentralfunktionen (wie OB1) ---
LangdruckToggle hauptschalter(4000UL, true);   // 4s Langdruck; Start = ein
LangdruckToggle alleLampen   (2000UL, false);  // 2s Langdruck (nur bei aktivem Hauptschalter)

void setup() {
  Serial.begin(115200);
  tS1.begin(); tS2.begin(); tS3.begin(); tS4.begin(); tS13.begin(); tS12.begin();
  L1.begin(); L2.begin(); L3.begin(); L4.begin(); L13.begin(); L12.begin();
  Serial.println(F("Lichtsteuerung gestartet"));
  // TODO: RTC (DS3231) initialisieren fuer Uhrzeit-Logik (Aussenlampe 16-22h)
  // TODO: Watchdog nach Test aktivieren: #include <avr/wdt.h> + wdt_enable(WDTO_2S)
}

void loop() {
  // 1) Eingaenge entprellt lesen
  bool s1 = tS1.read(), s2 = tS2.read(), s3 = tS3.read(), s4 = tS4.read();
  bool s13 = tS13.read(), s12 = tS12.read();

  // 2) Sammelsignal fuer die Zentralfunktion
  //    (PIR/Ventilator-Eingaenge bewusst NICHT aufnehmen -> hier z.B. s12 weglassen)
  bool sammel = s1 || s2 || s3 || s4 || s13;

  // 3) Zentralfunktionen
  bool haupt = hauptschalter.update(sammel);
  bool alle  = alleLampen.update(sammel, haupt);

  // 4) Verbraucher aktualisieren
  L1.update(s1, haupt, alle);
  L2.update(s2, haupt, alle);
  L3.update(s3, haupt, alle);
  L4.update(s4, haupt, alle);
  L13.update(s13, haupt, alle);
  L12.update(s12, haupt);              // Schalter-Modus: Nachlauf, kein "Alle Lampen"

  // ----------------------------------------------------------------------
  // TODO - Sonderschaltungen aus dem SPS-"Aufruf" nachbilden:
  //  - Gang: bool gangEin = L_gang.update((pir && helligkeit < schwelle), haupt);
  //          digitalWrite(NOTLICHT_PIN, !gangEin);            // Notlicht invertiert
  //  - Rollladen: L7 = Fahren (Zeit), L8 = Richtung (S/R) - mit Verriegelung
  //  - Aussenlampe: (helligkeit < 4 && stundeZwischen(16,22)) || tasterManuell
  //  - Helligkeit: Impulse an einem Interrupt-Pin zaehlen -> Faktor/Offset
  // ----------------------------------------------------------------------

  // kurze Zykluspause; blockiert nicht kritisch (Lichtlogik ist langsam)
  delay(10);
  // wdt_reset();   // sobald Watchdog aktiv
}
