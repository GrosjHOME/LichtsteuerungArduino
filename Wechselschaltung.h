#ifndef WECHSELSCHALTUNG_H
#define WECHSELSCHALTUNG_H
#include <Arduino.h>

// ============================================================================
//  Wiederverwendbare Bausteine fuer die Lichtsteuerung (Portierung STEP7 -> C++)
//  Alle Zeiten in Millisekunden. millis()-Vergleiche sind rollover-sicher
//  (Ueberlauf nach ~49,7 Tagen wird korrekt behandelt).
// ============================================================================

static const uint8_t NO_PIN = 255;   // logischer Ausgang (kein physischer Pin)

// --- Einschaltverzoegerung (entspricht TON) -------------------------------
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
class Tof {
  bool q = false;
  bool prev = false;
  unsigned long tOff = 0;
public:
  bool update(bool in, unsigned long pt) {
    if (in) {
      q = true;
    } else {
      if (prev) tOff = millis();
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

// --- Entprellung (quellen-unabhaengig: bekommt den Roh-Zustand herein) -----
//  Dieselbe Klasse dient fuer native Pins UND MCP23017-Kanaele; das Rohsignal
//  wird von aussen (readS(...)) geliefert, die Polaritaet dort behandelt.
class DebIn {
  unsigned long db;
  bool state = false, raw = false;
  unsigned long tCh = 0;
public:
  DebIn(unsigned long debounceMs = 30) : db(debounceMs) {}
  bool update(bool rawActive) {
    if (rawActive != raw) { raw = rawActive; tCh = millis(); }
    if ((unsigned long)(millis() - tCh) >= db) state = raw;
    return state;
  }
};

// --- Kern-Baustein: Wechselschaltung (entspricht dem STEP7-FB) ------------
//  Taster-Modus  (schalter=false): Tastendruck toggelt; Auto-Aus nach maxZeit;
//                 AlleLampenEin schaltet zusaetzlich ein; Hauptschalter gibt frei.
//  Schalter-Modus(schalter=true) : Ausgang folgt Eingang mit Nachlauf asZeit;
//                 Hauptschalter-gegated; maxZeit/AlleLampen ohne Wirkung.
//  outPin == NO_PIN -> nur logischer Ausgang (Rueckgabewert), kein Pin.
class Wechselschaltung {
  uint8_t outPin;
  bool schalter;
  unsigned long asZeit;
  unsigned long maxZeit;
  bool ml1 = false;
  bool lampeEin = false;
  REdge edge;
  Ton  maxTimer;
  Tof  nachlauf;
public:
  Wechselschaltung(uint8_t pin, bool schalter_, unsigned long asZeit_, unsigned long maxZeit_)
    : outPin(pin), schalter(schalter_), asZeit(asZeit_), maxZeit(maxZeit_) {}

  void begin() { if (outPin != NO_PIN) { pinMode(outPin, OUTPUT); digitalWrite(outPin, LOW); } }
  void setAsZeit(unsigned long v)  { asZeit = v; }    // dynamische Nachlaufzeit (z.B. Scheibenheizung)
  void setMaxZeit(unsigned long v) { maxZeit = v; }   // dynamische Max-Zeit (z.B. Rollladen-Fahrzeit)

  bool update(bool iSchalter, bool hauptschalter, bool alleLampenEin = false) {
    bool q;
    if (schalter) {
      q = nachlauf.update(iSchalter, asZeit) && hauptschalter;
    } else {
      bool maxErreicht = maxTimer.update(lampeEin, maxZeit);
      if (edge.rising(iSchalter || maxErreicht)) ml1 = !ml1;
      lampeEin = (ml1 || alleLampenEin) && hauptschalter;
      q = lampeEin;
    }
    if (outPin != NO_PIN) digitalWrite(outPin, q ? HIGH : LOW);
    return q;
  }
};

// --- Zentralfunktion: Langdruck-Toggle (Hauptschalter / Alle Lampen, OB1) --
class LangdruckToggle {
  unsigned long haltezeit;
  Ton   timer;
  REdge edge;
  bool  zustand;
public:
  LangdruckToggle(unsigned long haltezeitMs, bool startZustand = false)
    : haltezeit(haltezeitMs), zustand(startZustand) {}
  bool update(bool sammel, bool freigabe = true) {
    if (edge.rising(timer.update(sammel && freigabe, haltezeit))) zustand = !zustand;
    return zustand;
  }
  bool get() const { return zustand; }
  void set(bool z) { zustand = z; }
};

// --- Uhrzeit (Sekunden des Tages) -----------------------------------------
//  Ersetzt READ_CLK/DT_TOD der SPS. Softwareuhr auf millis()-Basis; muss per
//  setze(...) synchronisiert werden (idealerweise beim Start aus einem DS3231).
//  Solange nicht gesetzt -> 0 (Zeitfenster damit inaktiv, nur Handbetrieb).
class Uhr {
  uint32_t basisSek = 0;
  unsigned long basisMs = 0;
  bool gesetzt = false;
public:
  void setze(uint8_t h, uint8_t m, uint8_t s) {
    basisSek = (uint32_t)h * 3600UL + (uint32_t)m * 60UL + s;
    basisMs = millis(); gesetzt = true;
  }
  uint32_t sekDesTages() {
    if (!gesetzt) return 0;
    unsigned long d = (millis() - basisMs) / 1000UL;
    return (uint32_t)((basisSek + d) % 86400UL);
  }
  bool istGesetzt() const { return gesetzt; }
};

#endif // WECHSELSCHALTUNG_H
