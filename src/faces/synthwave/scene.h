#pragma once
// Scena nad horyzontem skladana z warstw w dowolnym prostokacie:
// niebo -> gwiazdy -> slonce (z przecieciami) -> data -> cyfry z glow.
// Kazda zmiana (cyfra, gwiazda, klatka slonca, glitch) = ponowne zlozenie
// tylko swojego prostokata, tlo pod spodem odtwarza sie samo.

#include <stdint.h>
#include "assets_gen.h"
#include "face_config.h"

class Scene {
public:
  static constexpr int DIGIT_SLOTS = 4;
  static constexpr uint8_t TRANSITION_STEPS = 6;
  static constexpr int MAX_DATE_CHARS = 32;

  void begin();

  // ---- cyfry: -1 = pusta (przed synchronizacja czasu) ----
  int8_t digit(int slot) const { return _slots[slot].to; }
  void setDigit(int slot, int8_t value, bool animate);
  bool slotAnimating(int slot) const { return _slots[slot].step != 0; }
  void advanceTransition(int slot);  // krok przejscia (wolac co klatke)

  // ---- data (UTF-8, wielkie litery Audiowide) ----
  void setDate(const char* utf8);

  // ---- animacje tla ----
  uint8_t sunFrame = 0;
  uint32_t starTick = 0;
  uint8_t starLevel(int i) const;  // jasnosc gwiazdy dla starTick

  // Sklada prostokat (y + h <= HORIZON_Y) do out (w*h pikseli, swap565)
  void compose(int x, int y, int w, int h, uint16_t* out) const;

  // Prostokat komorki cyfry
  static int slotX(int slot) { return assets::DIGIT_X[slot]; }

private:
  struct Slot {
    int8_t from = -1, to = -1;
    uint8_t step = 0;  // 0 = spoczynek (pokazuje "to")
  };
  Slot _slots[DIGIT_SLOTS];

  struct DateChar {
    int16_t x;
    int8_t glyph;  // index w DATE_GLYPHS, -1 = brak
  };
  DateChar _date[MAX_DATE_CHARS];
  int _dateLen = 0;

  void drawGlyph(const uint8_t* data, int gw, int gx, int gy, int jitterSeed, uint8_t step,
                 int x0, int y0, int w, int h, uint16_t* out) const;
  void drawDate(int x0, int y0, int w, int h, uint16_t* out) const;
};

extern Scene scene;
