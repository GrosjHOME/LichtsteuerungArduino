#ifndef WECHSELSCHALTUNG_H
#define WECHSELSCHALTUNG_H
#include <Arduino.h>

// ============================================================================
//  Wiederverwendbare Bausteine fuer die Lichtsteuerung (Portierung STEP7 -> C++)
//  Alle Zeiten in Millisekunden. millis()-Vergleiche sind rollover-sicher
//  (Ueberlauf nach ~49,7 Tagen wird korrekt behandelt).
// ============================================================================

// --- Einschaltverzoegerung (entspricht TON) -------------------------------
// Q = true, wenn IN ununterbrochen fuer PT ms true war.
class Ton {
  bool running = false;
  unsigned long t0 = 0;
public:
  bool update(bool in, unsigned long pt) {
    if (!in) { running = false; return false; }
    if (!running) { running = true; t0 = millis(); }
    return (unsigned long)(millis() - t0) >= pt;
  }
};

// --- Ausschaltverzoegerung / Nachlauf (entspricht TOF) --------------------
// Q folgt IN; nach fallender Flanke bleibt Q noch PT ms true.
class Tof {
  bool q = false;
  bool prev = false;
  unsigned long tOff = 0;
public:
  bool update(bool in, unsigned long pt) {
    if (in) {
      q = true;
    } else {
      if (prev) tOff = millis();                         // fallende Flanke merken
      if (q && (unsigned long)(millis() - tOff) >= pt) q = false;
    }
    prev = in;
    return q;
  }
};

// --- Steigende Flanke (entspricht FP) -------------------------------------
class REdge {
  bool prev = false;
public:
  bool rising(bool in) { bool r = in && !prev; prev = in; return r; }
};

// --- Entprellter Eingang --------------------------------------------------
// activeLow=true  -> Taster/Schalter gegen GND (INPUT_PULLUP)
// activeLow=false -> Signal fuehrt +Pegel (INPUT; externen Pulldown vorsehen!)
// WICHTIG: Polaritaet an die reale Verdrahtung der Vorschalt-Relais anpassen.
class DebIn {
  uint8_t pin;
  unsigned long db;
  bool activeLow;
  bool raw = false, state = false;
  unsigned long tCh = 0;
public:
  DebIn(uint8_t p, bool activeLow_ = true, unsigned long debounceMs = 30)
    : pin(p), db(debounceMs), activeLow(activeLow_) {}
  void begin() { pinMode(pin, activeLow ? INPUT_PULLUP : INPUT); }
  bool read() {
    bool r = (digitalRead(pin) == (activeLow ? LOW : HIGH));
    if (r != raw) { raw = r; tCh = millis(); }              // Zustandswechsel -> Zeit neu
    if ((unsigned long)(millis() - tCh) >= db) state = raw; // stabil laenger als db -> uebernehmen
    return state;
  }
};

// --- Kern-Baustein: Wechselschaltung (entspricht dem STEP7-FB) ------------
//  Taster-Modus  (schalter=false): Tastendruck toggelt den Ausgang;
//                 Auto-Aus nach maxZeit; AlleLampenEin schaltet zusaetzlich
//                 ein; Hauptschalter gibt frei.
//  Schalter-Modus(schalter=true) : Ausgang folgt dem Eingang mit Nachlauf
//                 asZeit; Hauptschalter-gegated; maxZeit/AlleLampen ohne Wirkung.
class Wechselschaltung {
  uint8_t outPin;
  bool schalter;            // == KeinImpuls im FB
  unsigned long asZeit;     // Nachlauf (Schalter-Modus)
  unsigned long maxZeit;    // max. Einschaltzeit (Taster-Modus)
  bool ml1 = false;         // Toggle-Merker (entspricht ML1)
  bool lampeEin = false;    // entspricht bLampeEIN
  REdge edge;
  Ton  maxTimer;
  Tof  nachlauf;
public:
  Wechselschaltung(uint8_t pin, bool schalter_, unsigned long asZeit_, unsigned long maxZeit_)
    : outPin(pin), schalter(schalter_), asZeit(asZeit_), maxZeit(maxZeit_) {}

  void begin() { pinMode(outPin, OUTPUT); digitalWrite(outPin, LOW); }

  // Gibt den Lampenzustand zurueck und setzt den physischen Ausgang.
  bool update(bool iSchalter, bool hauptschalter, bool alleLampenEin = false) {
    bool q;
    if (schalter) {
      q = nachlauf.update(iSchalter, asZeit) && hauptschalter;
    } else {
      bool maxErreicht = maxTimer.update(lampeEin, maxZeit);   // MAXTIME(IN:=bLampeEIN)
      if (edge.rising(iSchalter || maxErreicht)) ml1 = !ml1;   // FP + XOR (Toggle)
      lampeEin = (ml1 || alleLampenEin) && hauptschalter;
      q = lampeEin;
    }
    digitalWrite(outPin, q ? HIGH : LOW);
    return q;
  }

  bool istEin() const { return schalter ? false : lampeEin; }
};

// --- Zentralfunktion: Langdruck-Toggle (entspricht Hauptschalter/AlleLampen in OB1) ---
//  Haelt man das Sammelsignal >= haltezeit gedrueckt, kippt der Zustand einmal.
class LangdruckToggle {
  unsigned long haltezeit;
  Ton   timer;
  REdge edge;
  bool  zustand;
public:
  LangdruckToggle(unsigned long haltezeitMs, bool startZustand = false)
    : haltezeit(haltezeitMs), zustand(startZustand) {}
  // sammel   = ODER der relevanten Taster (PIR/Sensoren bewusst ausnehmen!)
  // freigabe = Vorbedingung (z.B. Hauptschalter fuer "Alle Lampen")
  bool update(bool sammel, bool freigabe = true) {
    if (edge.rising(timer.update(sammel && freigabe, haltezeit))) zustand = !zustand;
    return zustand;
  }
  bool get() const { return zustand; }
  void set(bool z) { zustand = z; }
};

#endif // WECHSELSCHALTUNG_H
