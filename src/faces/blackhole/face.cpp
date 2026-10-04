// Tarcza "blackhole": czarna dziura z dyskiem akrecyjnym (klimat
// Interstellar). Kazdy pierscien dysku kreci sie z wlasna predkoscia
// (wewnetrzne szybciej), strona nadlatujaca jasniejsza (Doppler), nad i pod
// cieniem soczewkowany obraz dalszej czesci dysku. Ponizej cyfry neon.
// Zmiana minuty = rozblysk dysku, pelna godzina = dlugi rozblysk + szybszy wir.
//
// Mapy pikseli (promien, kat, jasnosc, poswiata) liczone na laptopie;
// tu tylko tekstura z przesunieciem obrotu per pierscien.

#include "core/face.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "assets_gen.h"
#include "faces/common_hd/hd.h"

using namespace assets;

namespace face {
namespace {

hd::Tiles tiles;
hd::TextLine dateLine;
char dateShown[48] = "";
int8_t digits[4] = {-1, -1, -1, -1};
int lastMinute = -1, lastHour = -1;

const hd::Style CLOCK_STYLE = {hd::Ink::Neon, LUT_CLOCK, 0, 0};
const hd::Style DATE_STYLE = {hd::Ink::Neon, LUT_DATE, 0, 0};

uint16_t rotSpeed[NR];  // 8.8, jednostki kata (256 = obrot) na klatke
uint16_t rot[NR];       // 8.8
uint8_t rotInt[NR];
int boost = 256, boostFrames = 0, spinFrames = 0;

inline uint16_t sample(uint16_t p, const uint8_t* m, int x, int y) {
  const uint8_t r = m[0];
  int I = TEX[r][uint8_t(m[1] + rotInt[r])] * m[2] >> 8;
  I = I * boost >> 8;
  if (I > 255) I = 255;
  return I ? hd::addLight(p, RAMP[r >> 3][I], x, y) : p;
}

void compose(int x0, int y0, int w, int h, uint16_t* out) {
  memset(out, 0, size_t(w) * h * 2);
  for (int r = 0; r < h; r++) {
    const int y = y0 + r;
    const bool inL = y >= L_Y && y < L_Y + L_H, inD = y >= D_Y && y < D_Y + D_H;
    if (!inL && !inD) continue;
    const bool front = y > CY;
    uint16_t* row = out + r * w;
    for (int x = x0; x < x0 + w; x++) {
      uint16_t p = 0;
      bool shadow = false;
      if (inL && x >= L_X && x < L_X + L_W) {
        const uint8_t* m = L_MAP[(y - L_Y) * L_W + (x - L_X)];
        if (m[0] == SHADOW) shadow = true;
        else {
          if (m[0] != NONE) p = sample(p, m, x, y);
          if (m[3]) p = hd::addLight(p, HAZE[m[3]], x, y);
        }
      }
      if (inD && x >= D_X && x < D_X + D_W) {
        const uint8_t* m = D_MAP[(y - D_Y) * D_W + (x - D_X)];
        if (m[0] != NONE && (front || !shadow)) p = sample(p, m, x, y);
        if (m[3]) p = hd::addLight(p, HAZE[m[3]], x, y);
      }
      row[x - x0] = p;
    }
  }
  for (const auto& s : STARS)
    if (s.x >= x0 && s.x < x0 + w && s.y >= y0 && s.y < y0 + h) {
      uint16_t& p = out[(s.y - y0) * w + s.x - x0];
      const uint8_t c[3] = {s.b, s.b, uint8_t(s.b * 7 / 8)};
      p = hd::addLight(p, c, s.x, s.y);
    }
  if (y0 < DIGIT_TOP + DIGIT_CELL_H && y0 + h > DIGIT_TOP) {
    for (int i = 0; i < 4; i++)
      if (digits[i] >= 0)
        hd::drawGlyph(DIGIT_DATA[digits[i]], DIGIT_CELL_W, DIGIT_CELL_H, DIGIT_X[i], DIGIT_TOP, CLOCK_STYLE, x0, y0, w,
                      h, out);
    if (digits[0] >= 0)
      hd::drawGlyph(COLON_DATA, COLON_W, DIGIT_CELL_H, COLON_X, DIGIT_TOP, CLOCK_STYLE, x0, y0, w, h, out);
  }
  if (y0 < DATE_BOTTOM && y0 + h > DATE_TOP) dateLine.draw(DATE_DATA, DATE_BASELINE, DATE_STYLE, x0, y0, w, h, out);
  for (int i = 0; i < w * h; i++) out[i] = px::swap(out[i]);
}

void updateClock(const struct tm* now) {
  int8_t nd[4] = {-1, -1, -1, -1};
  char date[48] = "";
  if (now) {
    nd[0] = now->tm_hour / 10;
    nd[1] = now->tm_hour % 10;
    nd[2] = now->tm_min / 10;
    nd[3] = now->tm_min % 10;
    snprintf(date, sizeof(date), "%s  %02d.%02d.%04d", WEEKDAYS[now->tm_wday], now->tm_mday, now->tm_mon + 1,
             now->tm_year + 1900);
    if (lastMinute >= 0 && now->tm_min != lastMinute) boostFrames = 14;
    if (lastHour >= 0 && now->tm_hour != lastHour) {
      boostFrames = 45;
      spinFrames = 90;
    }
    lastMinute = now->tm_min;
    lastHour = now->tm_hour;
  }
  for (int i = 0; i < 4; i++)
    if (nd[i] != digits[i]) {
      if (digits[i] < 0 || nd[i] < 0) tiles.mark(COLON_X, DIGIT_TOP, COLON_W, DIGIT_CELL_H);
      digits[i] = nd[i];
      tiles.mark(DIGIT_X[i], DIGIT_TOP, DIGIT_CELL_W, DIGIT_CELL_H);
    }
  if (strcmp(date, dateShown) != 0) {
    strcpy(dateShown, date);
    dateLine.set(date, DATE_GLYPHS, DATE_GLYPH_COUNT, SCREEN_W / 2);
    tiles.mark(0, DATE_TOP, SCREEN_W, DATE_BOTTOM - DATE_TOP);
  }
}

}  // namespace

void begin() {
  // wewnetrzny pierscien: obrot w ~6 s, zewnetrzne wolniej (R^-1.5)
  for (int r = 0; r < NR; r++) {
    const float R = 54.0f + (r + 0.5f) / NR * (225.0f - 54.0f);
    rotSpeed[r] = uint16_t(3.9f * powf(54.0f / R, 1.5f) * 256);
    rot[r] = uint16_t(r * 9973);  // rozne fazy startowe
  }
}

void drawAll() {
  tiles.markAll();
  tiles.flush(compose);
}

void frame(uint32_t nowMs, const struct tm* now) {
  updateClock(now);
  const int mult = spinFrames ? 3 : 1;
  if (spinFrames) spinFrames--;
  for (int r = 0; r < NR; r++) {
    rot[r] += rotSpeed[r] * mult;
    rotInt[r] = rot[r] >> 8;
  }
  // rozblysk: szybko w gore, powoli w dol
  if (boostFrames) {
    boostFrames--;
    boost = 256 + (boostFrames > 8 ? 220 : boostFrames * 27);
  } else {
    boost = 256;
  }
  tiles.mark(L_X, L_Y, L_W, L_H);
  tiles.mark(D_X, D_Y, D_W, D_H);
  tiles.flush(compose);
}

}  // namespace face
