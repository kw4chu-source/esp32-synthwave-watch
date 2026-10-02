#pragma once
// Scena vhs_hallway: tlo VHS (z wariantem zgaszonej swietlowki) -> postac
// (klatka = minuta) -> cyfry, data, REC -> pas zaklocen tasmy.

#include <stdint.h>
#include "assets_gen.h"
#include "config.h"

namespace scene {

constexpr int BAND_H = 10;  // wysokosc pasa zaklocen tasmy

struct State {
  int8_t digits[4] = {-1, -1, -1, -1};
  uint8_t figure = 0;     // klatka postaci (minuta)
  bool lampOff = false;
  bool recOn = true;
  bool black = false;     // czarny ekran (po "skoku" o pelnej godzinie)
  int bandY = -100;       // gorny wiersz pasa zaklocen; poza ekranem = brak
  uint32_t noiseSeed = 0; // zmienia sie co klatke - szum w pasie
};

extern State st;

void setDate(const char* utf8);

// Prostokat zajmowany przez date (do dirty rect)
void dateRect(int& x, int& y, int& w, int& h);

void compose(int x, int y, int w, int h, uint16_t* out);

}  // namespace scene
