#pragma once
// Scena nad horyzontem skladana z warstw w dowolnym prostokacie:
// niebo -> gwiazdy -> slonce (z przecieciami) -> chmury -> opad -> data
// (+ pogoda) -> cyfry z glow.
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

  // ---- pogoda ----
  struct Cloud {
    int16_t x, y;
    uint8_t shape;
  };
  static constexpr int MAX_CLOUDS = 4;
  Cloud clouds[MAX_CLOUDS];
  int cloudCount = 0;
  uint8_t skyDim = 255;  // przyciemnienie nieba i slonca (255 = brak)
  bool fog = false;
  int8_t wxIcon = -1;    // -1 = brak danych

  struct Drop {
    float x, y, vy;
    uint8_t len;
  };
  static constexpr int MAX_DROPS = 90;
  Drop drops[MAX_DROPS];
  int dropCount = 0;
  bool snow = false;
  float slope = 0.3f;
  static constexpr int BOLT_POINTS = 12;
  int16_t boltX[BOLT_POINTS];
  bool bolt = false;

  void setWeatherText(const char* utf8);  // temperatura po prawej (pusty = brak)
  void dropBounds(const Drop& d, int& x0, int& y0, int& x1, int& y1) const;
  void cloudRect(int i, int& x, int& y, int& w, int& h) const;
  // Opad i piorun w prostokacie (out przed swap) - tez dla siatki ponizej horyzontu
  void drawPrecip(int x0, int y0, int w, int h, uint16_t* out) const;

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
  DateChar _wx[8];
  int _wxLen = 0;
  int _wxIconX = 0;

  void drawGlyph(const uint8_t* data, int gw, int gx, int gy, int jitterSeed, uint8_t step,
                 int x0, int y0, int w, int h, uint16_t* out) const;
  void drawDate(int x0, int y0, int w, int h, uint16_t* out) const;
};

extern Scene scene;
