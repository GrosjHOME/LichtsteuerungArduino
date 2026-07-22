// ============================================================================
//  Lichtsteuerung (Arduino Mega) - vollstaendige Portierung der STEP7-Logik
//  (Haus Wittenbach). Funktioniert 1:1 wie die SPS-Version:
//   - je Verbraucher eine Wechselschaltung (Taster-Toggle / Schalter-Nachlauf)
//   - Zentralfunktionen Hauptschalter / Alle Lampen (Langdruck, wie OB1)
//   - Gang OG mit PIR + Helligkeit + Notlicht
//   - Gang EG (3 Taster) + Notlicht + Kueche/Gang-Kopplung
//   - Kueche Insel, von Gang (S15) ausschaltbar
//   - Rollladen (Fahren + Richtung, Fahrzeiten)
//   - Scheibenheizung Bad + Ventilator (gekoppelt)
//   - Aussenlampe daemmerungs-/zeitabhaengig + manuell
//   - Helligkeitsmessung (Impulszaehlung)
//
//  Pinbelegung unten an die reale Verdrahtung anpassen! (Vorschalt-Relais)
//  Fuer sauberere Verdrahtung ggf. I/O-Expander (MCP23017) - Logik bleibt gleich.
// ============================================================================

#include "Wechselschaltung.h"

// ----- Zeit-Konstanten (ms) -----
const unsigned long SEK = 1000UL, MINUTE = 60000UL, STUNDE = 3600000UL;

// ----- Helligkeits-Schwellen (kleiner = dunkler), aus der SPS -----
const int H_GANG   = 33;   // Gang OG Grundlicht an
const int H_GANG2  = 17;   // Gang OG zusaetzliche Lampen
const int H_AUSSEN = 4;    // Aussenlampe

// ----- Pinbelegung (ANPASSEN!) -----
// Ausgaenge L1..L32 -> Pins 22..53
const uint8_t OUT[33] = { 0,
  22,23,24,25,26,27,28,29,30,31, 32,33,34,35,36,37,38,39,40,41,
  42,43,44,45,46,47,48,49,50,51, 52,53 };
// Eingaenge S1..S31 (S32 = Helligkeitsimpuls, separat am Interruptpin)
// (13 = Onboard-LED, 20/21 = I2C fuer RTC, 2/3 = Interrupt -> ausgespart)
const uint8_t IN[32] = { 0,
  4,5,6,7,8,9,10,11,12,14, 15,16,17,18,19,54,55,56,57,58,
  59,60,61,62,63,64,65,66,67,68, 69 };
const uint8_t PIN_HELL = 2;   // S32 Helligkeitsimpuls (INT0-faehig)

// ============================================================================
//  Verbraucher-Objekte (entsprechen dem VAR-Block von FB "Aufruf")
// ============================================================================
// Einfache Taster (Ausgangspin, schalter=false, asZeit=0, maxZeit)
Wechselschaltung wL1 (OUT[1],  false, 0, 1*STUNDE);   // Lampe Zimmer Mitte OG   <- S1
Wechselschaltung wL2 (OUT[2],  false, 0, 1*STUNDE);   // Steckdose Sued OG        <- S2
Wechselschaltung wL3 (OUT[3],  false, 0, 1*STUNDE);   // Lampe Suedost OG         <- S3
Wechselschaltung wL4 (OUT[4],  false, 0, 1*STUNDE);   // Lampe Suedwest OG        <- S4
Wechselschaltung wL5 (OUT[5],  false, 0, 1*STUNDE);   // Steckdose Suedwest OG    <- S5
Wechselschaltung wL6 (OUT[6],  false, 0, 1*STUNDE);   // Steckdose Mitte OG       <- S6
Wechselschaltung wL9 (OUT[9],  false, 0, 1*STUNDE);   // Steckdose Nordost OG     <- S9
Wechselschaltung wL10(OUT[10], false, 0, 1*STUNDE);   // Lampe Nordost OG         <- S10
Wechselschaltung wL13(OUT[13], false, 0, 2*STUNDE);   // Lampen Douche Bad OG     <- S13
Wechselschaltung wL14(OUT[14], false, 0, 2*STUNDE);   // Lampen Spiegel Bad OG    <- S14
Wechselschaltung wL16(OUT[16], false, 0, 3*STUNDE);   // Steckdose Nordost EG     <- S16
Wechselschaltung wL17(OUT[17], false, 0, 3*STUNDE);   // Lampe Nordost EG         <- S17
Wechselschaltung wL18(OUT[18], false, 0, 3*STUNDE);   // Steckdose Mitteost EG    <- S18
Wechselschaltung wL19(OUT[19], false, 0, 3*STUNDE);   // Lampe Mitteost EG        <- S19
Wechselschaltung wL22(OUT[22], false, 0, 5*STUNDE);   // Steckdose Wohnzimmer     <- S22
Wechselschaltung wL23(OUT[23], false, 0, 5*STUNDE);   // Lampe oberhalb Ofen      <- S21
Wechselschaltung wL24(OUT[24], false, 0, 20*MINUTE);  // Vorraum Nordwest EG      <- S23
Wechselschaltung wL28(OUT[28], false, 0, 20*MINUTE);  // Lampe WC EG              <- S26
Wechselschaltung wL31(OUT[31], false, 0, 4*STUNDE);   // Kueche Esstisch EG       <- S30

// Schalter / Nachlauf
Wechselschaltung wL27(OUT[27], true, 20*MINUTE, 0);   // Ventilator WC EG         <- S25
Wechselschaltung wL12(OUT[12], true, 30*MINUTE, 0);   // Ventilator Bad OG        <- bVentilatoScheibenHeiz
Wechselschaltung wL32(OUT[32], true, 10*MINUTE, 0);   // Scheibenheizung Bad OG   (asZeit dyn. 40/10min)

// Sonderschaltungen
Wechselschaltung wGangEG(OUT[25], false, 0, 10*MINUTE);  // Gang EG (L25) <- S24|S15|S29
Wechselschaltung wKueche(OUT[29], false, 0, 4*STUNDE);   // Kueche Insel (L29) <- S27|S28|(S15&L29)
Wechselschaltung wGangOG(NO_PIN, true, 45*SEK, 0);       // Gang OG (bNL15) <- S11&dunkel
Wechselschaltung wAussen(NO_PIN, false, 0, 2*STUNDE);    // Aussen manuell (bL21) <- S20
Wechselschaltung wStore (OUT[7], false, 0, 11*SEK);      // Rollladen Fahren (L7), Fahrzeit dyn.

// ----- Eingaenge -----
DebIn taste[32];                 // taste[1..31]  (Polaritaet ggf. auf activeLow=false aendern)

// ----- Zentralfunktionen / Uhr -----
LangdruckToggle hauptschalter(4*SEK, true);    // 4s Langdruck; Start = ein
LangdruckToggle alleLampen   (2*SEK, false);   // 2s Langdruck (bei Hauptschalter)
Uhr uhr;
Ton storeStartTon;                             // 100ms-Startverzoegerung Rollladen

// ----- Helligkeit (Impulszaehlung, ersetzt Zaehler Z0 / OB35) -----
volatile unsigned long g_hellPulse = 0;
int  helligkeit = 1000;                         // aktueller Wert (hoch = hell)
unsigned long tHellLast = 0;
const unsigned long HELL_INTERVALL = 1000UL;    // Auswerteintervall
const float HELL_FAKTOR = 1.0;                  // Setup_k1 (SPS)
const int   HELL_OFFSET = 0;

void onHellPulse() { g_hellPulse++; }

// ----- Zustaende, die ueber Zyklen gehalten werden -----
bool storeDirAb = false;         // L8: Richtung (false=auf, true=ab)
unsigned long tStore = 11*SEK;   // aktuelle Fahrzeit

void setup() {
  Serial.begin(115200);
  for (uint8_t n = 1; n <= 31; n++) { taste[n].attach(IN[n]); taste[n].begin(); }

  wL1.begin();  wL2.begin();  wL3.begin();  wL4.begin();  wL5.begin();  wL6.begin();
  wL9.begin();  wL10.begin(); wL13.begin(); wL14.begin(); wL16.begin(); wL17.begin();
  wL18.begin(); wL19.begin(); wL22.begin(); wL23.begin(); wL24.begin(); wL28.begin();
  wL31.begin(); wL27.begin(); wL12.begin(); wL32.begin();
  wGangEG.begin(); wKueche.begin(); wStore.begin();
  // abgeleitete/kombinierte Ausgaenge direkt als OUTPUT
  pinMode(OUT[8],  OUTPUT); pinMode(OUT[11], OUTPUT); pinMode(OUT[15], OUTPUT);
  pinMode(OUT[20], OUTPUT); pinMode(OUT[21], OUTPUT); pinMode(OUT[26], OUTPUT);
  pinMode(OUT[30], OUTPUT);
  digitalWrite(OUT[20], LOW);   // L20 unbenutzt

  pinMode(PIN_HELL, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PIN_HELL), onHellPulse, RISING);

  // Uhrzeit setzen: idealerweise aus einem DS3231 (RTClib):
  //   #include <RTClib.h>  RTC_DS3231 rtc;  rtc.begin();
  //   DateTime t = rtc.now(); uhr.setze(t.hour(), t.minute(), t.second());
  // Ohne RTC bleiben die Zeitfenster inaktiv (nur Handbetrieb).

  Serial.println(F("Lichtsteuerung gestartet"));
}

void loop() {
  // ---------- Eingaenge lesen ----------
  bool s[32];
  for (uint8_t n = 1; n <= 31; n++) s[n] = taste[n].read();

  // ---------- Helligkeit auswerten (wie OB35) ----------
  if ((unsigned long)(millis() - tHellLast) >= HELL_INTERVALL) {
    noInterrupts(); unsigned long c = g_hellPulse; g_hellPulse = 0; interrupts();
    helligkeit = (int)(c * HELL_FAKTOR) + HELL_OFFSET;
    tHellLast = millis();
  }
  uint32_t sek = uhr.sekDesTages();

  // ---------- Zentralfunktionen (OB1) ----------
  // Sammelsignal: S1-S10, S13-S24, S26-S30 (PIR S11, Ventis S12/S25 ausgenommen)
  bool sammel = s[1]||s[2]||s[3]||s[4]||s[5]||s[6]||s[7]||s[8]||s[9]||s[10]
             || s[13]||s[14]||s[15]||s[16]||s[17]||s[18]||s[19]||s[20]||s[21]||s[22]
             || s[23]||s[24]||s[26]||s[27]||s[28]||s[29]||s[30];
  bool haupt = hauptschalter.update(sammel);
  bool alle  = alleLampen.update(sammel, haupt);

  // ---------- Gruppe 1 OG: einfache Verbraucher ----------
  wL1.update(s[1], haupt, alle);
  wL2.update(s[2], haupt, alle);
  wL3.update(s[3], haupt, alle);
  wL4.update(s[4], haupt, alle);
  wL5.update(s[5], haupt, alle);
  wL6.update(s[6], haupt, alle);

  // ---------- Rollladen (Store) Suedwest OG ----------
  if (s[7]) storeDirAb = false;                 // S7 -> auf  (R L8)
  if (s[8]) storeDirAb = true;                  // S8 -> ab   (S L8)
  bool storeStart = storeStartTon.update(s[7] || s[8], 100);
  if (s[7]) tStore = 11*SEK; else if (s[8]) tStore = 42*SEK;
  wStore.setMaxZeit(tStore);
  wStore.update(storeStart, haupt, false);      // L7 = Fahren
  digitalWrite(OUT[8], storeDirAb ? HIGH : LOW);// L8 = Richtung

  // ---------- Scheibenheizung + Ventilator Bad OG ----------
  // Ausloesung durch Kalt-/Feuchte-Sensor S31 oder Handschalter S12.
  // (Bereinigt: die SPS hatte hier ein fehlerhaftes 5-Sekunden-Zeitfenster um
  //  10:00:05-10 Uhr, das den S31-Pfad praktisch wirkungslos machte.)
  bool bVenti = s[31] || s[12];
  wL32.setAsZeit(s[31] ? 40*MINUTE : 10*MINUTE);
  wL32.update(bVenti, haupt);                   // Scheibenheizung L32
  wL12.update(bVenti, haupt);                   // Ventilator Bad L12

  // ---------- Gruppe 2 OG ----------
  wL9.update(s[9], haupt, alle);
  wL10.update(s[10], haupt, alle);

  // Gang OG: PIR (S11) + Helligkeit; Nachlauf 45s
  bool s11undDunkel = (helligkeit < H_GANG) && s[11];
  bool bNL15 = wGangOG.update(s11undDunkel, haupt);   // logisch
  digitalWrite(OUT[15], bNL15 ? LOW : HIGH);          // L15 Notlicht = NICHT Ganglicht
  digitalWrite(OUT[11], (bNL15 && (helligkeit < H_GANG2)) ? HIGH : LOW); // L11 Zusatzlampen

  wL13.update(s[13], haupt, alle);
  wL14.update(s[14], haupt, alle);

  // ---------- Gruppe 3 EG ----------
  wL16.update(s[16], haupt, alle);
  wL17.update(s[17], haupt, alle);
  wL18.update(s[18], haupt, alle);
  wL19.update(s[19], haupt, alle);

  // ---------- Gruppe 4 EG ----------
  wL28.update(s[26], haupt, alle);              // Lampe WC   <- S26
  wL27.update(s[25], haupt);                    // Ventilator WC (Schalter) <- S25

  // Gang EG (3 Taster) + Notlicht + Kueche/Gang
  bool mEingangK = s[24] || s[15] || s[29];
  bool l25 = wGangEG.update(mEingangK, haupt, alle);
  digitalWrite(OUT[26], l25 ? LOW : HIGH);      // L26 Notlicht = NICHT L25
  digitalWrite(OUT[30], l25 ? HIGH : LOW);      // L30 Kueche/Gang folgt L25

  wL23.update(s[21], haupt, alle);              // Lampe oberhalb Ofen <- S21

  // Kueche Insel: S27|S28 zum Schalten, S15 (Gang) schaltet nur AUS solange an
  static bool l29 = false;
  bool mEingangH = s[27] || s[28] || (s[15] && l29);
  l29 = wKueche.update(mEingangH, haupt, alle);

  wL31.update(s[30], haupt, alle);              // Kueche Esstisch <- S30
  wL24.update(s[23], haupt, alle);              // Vorraum Nordwest <- S23

  // Steckdose Wohnzimmer (wird fuer Aussenlampen-Logik gebraucht -> zuerst)
  bool l22 = wL22.update(s[22], haupt, alle);

  // Aussenlampe Sued: manuell (S20) ODER (dunkel & 16-22h & Wohnzimmer-Steckdose aus)
  bool bL21 = wAussen.update(s[20], haupt, alle);
  bool autoAussen = (helligkeit < H_AUSSEN)
                 && (sek < 22UL*3600UL) && (sek > 16UL*3600UL) && !l22;
  digitalWrite(OUT[21], (autoAussen || bL21) ? HIGH : LOW);   // L21

  delay(10);
  // TODO Dauerbetrieb: Watchdog aktivieren (avr/wdt) + wdt_reset() hier.
}
