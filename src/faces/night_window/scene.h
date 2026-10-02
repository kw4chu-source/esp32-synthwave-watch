#pragma once
// Scena night_window: tlo (latarnia wl./wyl.) -> zapalone okno naprzeciwko ->
// postac -> deszcz za szyba -> krople na szybie -> odbicie budzika.

#include <stdint.h>
#include "assets_gen.h"
#include "config.h"

namespace scene {

struct Rain {     // deszcz za szyba
  float x, y, vy;
  uint8_t len;
};
struct Runner {   // kropla splywajaca po szybie
  float x, y;
  uint8_t r;
};

constexpr int RAIN_COUNT = 36;
constexpr int RUNNER_COUNT = 7;
constexpr float RAIN_SLOPE = 0.12f;

struct State {
  int8_t digits[4] = {-1, -1, -1, -1};
  int8_t figure = 0;        // -1 = nikogo nie ma
  bool lampOff = false;
  bool litOff = false;      // okno naprzeciwko zgaszone
  Rain rain[RAIN_COUNT];
  Runner runners[RUNNER_COUNT];
};

extern State st;

void setDate(const char* utf8);
void rainBounds(const Rain& r, int& x0, int& y0, int& x1, int& y1);

void compose(int x, int y, int w, int h, uint16_t* out);

}  // namespace scene
