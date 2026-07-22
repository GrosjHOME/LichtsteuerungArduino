// ============================================================================
//  Lichtsteuerung (Arduino Mega) - vollstaendige Portierung der STEP7-Logik
//  (Haus Wittenbach).
//
//  Hardware-Konzept:
//   - Ausgaenge (L1..L32): 4x 8-Kanal-Relaismodul DIREKT an native Pins (active-HIGH)
//   - Eingaenge (S1..S31): 1x DIN-Board mit 2x MCP23017 (I2C, Schraubklemmen)
//   - Helligkeitsimpuls (S32): nativer Interrupt-Pin (D2)
//
//  E/A-Belegung siehe README.
// ============================================================================

#include <Wire.h>
#include <Adafruit_MCP23X17.h>
#include "Wechselschaltung.h"

// ----- Zeit-Konstanten (ms) -----
const unsigned long SEK = 1000UL, MINUTE = 60000UL, STUNDE = 3600000UL;

// ----- Helligkeits-Schwellen (kleiner = dunkler), aus der SPS -----
const int H_GANG   = 33;   // Gang OG Grundlicht
const int H_GANG2  = 17;   // Gang OG Zusatzlampen
const int H_AUSSEN = 4;    // Aussenlampe

// ----- Ausgaenge L1..L32 -> native Arduino-Pins (active-HIGH) -----
const uint8_t OUT[33] = { 0,
  22,23,24,25,26,27,28,29,30,31, 32,33,34,35,36,37,38,39,40,41,
  42,43,44,45,46,47,48,49,50,51, 52,53 };

// ----- Eingaenge ueber 2x MCP23017 (I2C) -----
Adafruit_MCP23X17 mcpH;              // Chip "High" (ICH): Klemmen HA0..HB7 -> S1..S16
Adafruit_MCP23X17 mcpL;              // Chip "Low"  (ICL): Klemmen LA0..LB7 -> S17..S31
const uint8_t MCP_ADDR_H = 0x20;    // per DIP-Schalter am Board einstellen!
const uint8_t MCP_ADDR_L = 0x21;
const bool INPUT_ACTIVE_LOW = true; // Kontakt gegen GND (INPUT_PULLUP). Bei +Signal: false.
uint16_t gH = 0xFFFF, gL = 0xFFFF;  // zuletzt gelesene GPIO-Register (idle = high)

const uint8_t PIN_HELL = 2;         // S32 Helligkeitsimpuls (nativ, INT0)

// S-Nr (1..31) -> entprellter Roh-Aktivzustand aus dem passenden MCP-Register
bool readS(uint8_t n) {
  uint16_t g = (n <= 16) ? gH : gL;         // S1..S16 = Chip H, S17..S31 = Chip L
  uint8_t bit = (n <= 16) ? (n - 1) : (n - 17);
  bool level = (g >> bit) & 0x1;            // 1 = HIGH
  return INPUT_ACTIVE_LOW ? !level : level;
}

// ============================================================================
//  Verbraucher-Objekte (entsprechen dem VAR-Block von FB "Aufruf")
// ============================================================================
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

Wechselschaltung wL27(OUT[27], true, 20*MINUTE, 0);   // Ventilator WC EG         <- S25
Wechselschaltung wL12(OUT[12], true, 30*MINUTE, 0);   // Ventilator Bad OG        <- bVenti
Wechselschaltung wL32(OUT[32], true, 10*MINUTE, 0);   // Scheibenheizung Bad OG   (asZeit dyn.)

Wechselschaltung wGangEG(OUT[25], false, 0, 10*MINUTE); // Gang EG (L25) <- S24|S15|S29
Wechselschaltung wKueche(OUT[29], false, 0, 4*STUNDE);  // Kueche Insel (L29) <- S27|S28|(S15&L29)
Wechselschaltung wGangOG(NO_PIN, true, 45*SEK, 0);      // Gang OG (bNL15) <- S11&dunkel
Wechselschaltung wAussen(NO_PIN, false, 0, 2*STUNDE);   // Aussen manuell (bL21) <- S20
Wechselschaltung wStore (OUT[7], false, 0, 11*SEK);     // Rollladen Fahren (L7)

// ----- Eingaenge (Entprellung) + Zentralfunktionen + Uhr -----
DebIn taste[32];                               // taste[1..31]
LangdruckToggle hauptschalter(4*SEK, true);    // 4s Langdruck; Start = ein
LangdruckToggle alleLampen   (2*SEK, false);   // 2s Langdruck (bei Hauptschalter)
Uhr uhr;
Ton storeStartTon;

// ----- Helligkeit (Impulszaehlung, ersetzt Zaehler Z0 / OB35) -----
volatile unsigned long g_hellPulse = 0;
int  helligkeit = 1000;
unsigned long tHellLast = 0;
const unsigned long HELL_INTERVALL = 1000UL;
const float HELL_FAKTOR = 1.0;
const int   HELL_OFFSET = 0;
void onHellPulse() { g_hellPulse++; }

// ----- gehaltene Zustaende -----
bool storeDirAb = false;
unsigned long tStore = 11*SEK;

void setup() {
  Serial.begin(115200);

  // Ausgaenge
  wL1.begin();  wL2.begin();  wL3.begin();  wL4.begin();  wL5.begin();  wL6.begin();
  wL9.begin();  wL10.begin(); wL13.begin(); wL14.begin(); wL16.begin(); wL17.begin();
  wL18.begin(); wL19.begin(); wL22.begin(); wL23.begin(); wL24.begin(); wL28.begin();
  wL31.begin(); wL27.begin(); wL12.begin(); wL32.begin();
  wGangEG.begin(); wKueche.begin(); wStore.begin();
  pinMode(OUT[8],  OUTPUT); pinMode(OUT[11], OUTPUT); pinMode(OUT[15], OUTPUT);
  pinMode(OUT[20], OUTPUT); pinMode(OUT[21], OUTPUT); pinMode(OUT[26], OUTPUT);
  pinMode(OUT[30], OUTPUT);
  digitalWrite(OUT[20], LOW);   // L20 unbenutzt

  // Eingaenge: MCP23017 initialisieren
  Wire.begin();
  if (!mcpH.begin_I2C(MCP_ADDR_H)) Serial.println(F("MCP H (ICH) nicht gefunden!"));
  if (!mcpL.begin_I2C(MCP_ADDR_L)) Serial.println(F("MCP L (ICL) nicht gefunden!"));
  for (uint8_t p = 0; p < 16; p++) {
    mcpH.pinMode(p, INPUT_ACTIVE_LOW ? INPUT_PULLUP : INPUT);
    mcpL.pinMode(p, INPUT_ACTIVE_LOW ? INPUT_PULLUP : INPUT);
  }

  // Helligkeitsimpuls
  pinMode(PIN_HELL, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PIN_HELL), onHellPulse, RISING);

  // Uhrzeit: idealerweise aus DS3231 (RTClib) setzen -> uhr.setze(h,m,s);
  // Ohne RTC bleiben die Zeitfenster inaktiv (nur Handbetrieb).

  Serial.println(F("Lichtsteuerung gestartet"));
}

void loop() {
  // ---------- Eingaenge lesen (beide MCP-Register je 1 I2C-Zugriff) ----------
  gH = mcpH.readGPIOAB();
  gL = mcpL.readGPIOAB();
  bool s[32];
  for (uint8_t n = 1; n <= 31; n++) s[n] = taste[n].update(readS(n));

  // ---------- Helligkeit auswerten (wie OB35) ----------
  if ((unsigned long)(millis() - tHellLast) >= HELL_INTERVALL) {
    noInterrupts(); unsigned long c = g_hellPulse; g_hellPulse = 0; interrupts();
    helligkeit = (int)(c * HELL_FAKTOR) + HELL_OFFSET;
    tHellLast = millis();
  }
  uint32_t sek = uhr.sekDesTages();

  // ---------- Zentralfunktionen (OB1) ----------
  bool sammel = s[1]||s[2]||s[3]||s[4]||s[5]||s[6]||s[7]||s[8]||s[9]||s[10]
             || s[13]||s[14]||s[15]||s[16]||s[17]||s[18]||s[19]||s[20]||s[21]||s[22]
             || s[23]||s[24]||s[26]||s[27]||s[28]||s[29]||s[30];
  bool haupt = hauptschalter.update(sammel);
  bool alle  = alleLampen.update(sammel, haupt);

  // ---------- Gruppe 1 OG ----------
  wL1.update(s[1], haupt, alle);
  wL2.update(s[2], haupt, alle);
  wL3.update(s[3], haupt, alle);
  wL4.update(s[4], haupt, alle);
  wL5.update(s[5], haupt, alle);
  wL6.update(s[6], haupt, alle);

  // ---------- Rollladen (Store) Suedwest OG ----------
  if (s[7]) storeDirAb = false;                 // S7 -> auf
  if (s[8]) storeDirAb = true;                  // S8 -> ab
  bool storeStart = storeStartTon.update(s[7] || s[8], 100);
  if (s[7]) tStore = 11*SEK; else if (s[8]) tStore = 42*SEK;
  wStore.setMaxZeit(tStore);
  wStore.update(storeStart, haupt, false);      // L7 = Fahren
  digitalWrite(OUT[8], storeDirAb ? HIGH : LOW);// L8 = Richtung

  // ---------- Scheibenheizung + Ventilator Bad OG ----------
  // Ausloesung durch Kalt-/Feuchte-Sensor S31 oder Handschalter S12 (SPS-Altlast bereinigt).
  bool bVenti = s[31] || s[12];
  wL32.setAsZeit(s[31] ? 40*MINUTE : 10*MINUTE);
  wL32.update(bVenti, haupt);                   // Scheibenheizung L32
  wL12.update(bVenti, haupt);                   // Ventilator Bad L12

  // ---------- Gruppe 2 OG ----------
  wL9.update(s[9], haupt, alle);
  wL10.update(s[10], haupt, alle);

  bool s11undDunkel = (helligkeit < H_GANG) && s[11];
  bool bNL15 = wGangOG.update(s11undDunkel, haupt);
  digitalWrite(OUT[15], bNL15 ? LOW : HIGH);                                  // L15 Notlicht
  digitalWrite(OUT[11], (bNL15 && (helligkeit < H_GANG2)) ? HIGH : LOW);      // L11 Zusatzlampen

  wL13.update(s[13], haupt, alle);
  wL14.update(s[14], haupt, alle);

  // ---------- Gruppe 3 EG ----------
  wL16.update(s[16], haupt, alle);
  wL17.update(s[17], haupt, alle);
  wL18.update(s[18], haupt, alle);
  wL19.update(s[19], haupt, alle);

  // ---------- Gruppe 4 EG ----------
  wL28.update(s[26], haupt, alle);              // Lampe WC   <- S26
  wL27.update(s[25], haupt);                    // Ventilator WC <- S25

  bool mEingangK = s[24] || s[15] || s[29];
  bool l25 = wGangEG.update(mEingangK, haupt, alle);
  digitalWrite(OUT[26], l25 ? LOW : HIGH);      // L26 Notlicht = NICHT L25
  digitalWrite(OUT[30], l25 ? HIGH : LOW);      // L30 Kueche/Gang folgt L25

  wL23.update(s[21], haupt, alle);              // Lampe oberhalb Ofen <- S21

  static bool l29 = false;
  bool mEingangH = s[27] || s[28] || (s[15] && l29);
  l29 = wKueche.update(mEingangH, haupt, alle);

  wL31.update(s[30], haupt, alle);              // Kueche Esstisch <- S30
  wL24.update(s[23], haupt, alle);              // Vorraum Nordwest <- S23

  bool l22 = wL22.update(s[22], haupt, alle);   // Steckdose Wohnzimmer

  bool bL21 = wAussen.update(s[20], haupt, alle);
  bool autoAussen = (helligkeit < H_AUSSEN)
                 && (sek < 22UL*3600UL) && (sek > 16UL*3600UL) && !l22;
  digitalWrite(OUT[21], (autoAussen || bL21) ? HIGH : LOW);   // L21

  delay(10);
}
