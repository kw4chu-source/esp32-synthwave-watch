// Tarcza "aurora": zorza polarna nad gorami - dwie warstwy kurtyn z
// pionowymi promieniami (zielen u dolu, fiolet u gory) faluja i przesuwaja
// sie, gwiazdy migocza, wszystko odbija sie w jeziorze. Cyfry neonowa
// zielen z cieniem (zeby nie razily). Zmiana minuty = meteor,
// pelna godzina = rozjasnienie zorzy + trzy meteory.
//
// Zorza liczona na zywo: co klatke parametry kolumn (podstawa, wysokosc,
// promienie), na piksel dwa odczyty LUT na kurtyne. Co klatke wysylany caly
// pas nieba i jeziora (y 12..292).

#include "core/face.h"

#include <esp_random.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "assets_gen.h"
#include "faces/common_hd/hd.h"

using namespace assets;

namespace face {
namespace {

constexpr int ANIM_TOP = 12, LAKE_END = 292, LAKE_ROWS = LAKE_END - HORIZON - 1;
constexpr int CURTAINS = 2;

hd::Tiles tiles;
hd::TextLine dateLine;
char dateShown[48] = "";
int8_t digits[4] = {-1, -1, -1, -1};
int lastMinute = -1, lastHour = -1;

const hd::Style CLOCK_STYLE = {hd::Ink::NeonShadow, LUT_CLOCK, 0, 0};
const hd::Style DATE_STYLE = {hd::Ink::Neon, LUT_DATE, 0, 0};

// LUT-y profilu pionowego (u = wysokosc nad podstawa w 1/64 kurtyny)
uint16_t lutUp[256], lutDown[64];
uint8_t aurCol[65][3];
uint8_t skyRow[HORIZON][3];
uint8_t lakeFade[LAKE_ROWS + 1];
int8_t lakeShift[LAKE_ROWS + 1];

// parametry kolumn liczone co klatke
int16_t base[CURTAINS][480];
uint16_t invH[CURTAINS][480];
uint16_t ray[CURTAINS][480];

float t = 0;
uint32_t frameNo = 0;
int brightFrames = 0;

struct Meteor {
  bool on;
  float x, y, vx, vy;
};
Meteor meteors[3];
int meteorQueue = 0;

uint32_t rnd(uint32_t n) { return esp_random() % n; }

// Kolor nieba z zorza w (x, y), y < HORIZON
inline void skyRgb(int x, int y, int& r, int& g, int& b) {
  if (y > RIDGE2[x]) {
    r = 2, g = 3, b = 8;
    return;
  }
  if (y > RIDGE[x]) {
    r = 4, g = 6, b = 14;
    return;
  }
  r = skyRow[y][0], g = skyRow[y][1], b = skyRow[y][2];
  for (int k = 0; k < CURTAINS; k++) {
    const int d = base[k][x] - y;
    uint32_t I;
    const uint8_t* c;
    if (d >= 0) {
      int u = (d * invH[k][x]) >> 8;
      if (u > 255) u = 255;
      I = lutUp[u];
      c = aurCol[u < 64 ? u : 64];
    } else {
      const int v = (-d * invH[k][x]) >> 8;
      if (v >= 64) continue;
      I = lutDown[v];
      c = aurCol[0];
    }
    I = (I * ray[k][x]) >> 8;
    r += (c[0] * I) >> 8;
    g += (c[1] * I) >> 8;
    b += (c[2] * I) >> 8;
  }
}

void drawMeteor(const Meteor& m, int x0, int y0, int w, int h, uint16_t* out) {
  const float len = sqrtf(m.vx * m.vx + m.vy * m.vy);
  const float ux = m.vx / len, uy = m.vy / len;
  for (int k = 0; k < 34; k++) {
    const int x = int(m.x - ux * k * 1.2f), y = int(m.y - uy * k * 1.2f);
    if (x < x0 || x >= x0 + w || y < y0 || y >= y0 + h || x < 0 || x >= SCREEN_W || y >= RIDGE[x]) continue;
    const int f = 255 - k * 7;
    const uint8_t c[3] = {uint8_t(f * 3 / 4), uint8_t(f), uint8_t(f)};
    uint16_t& p = out[(y - y0) * w + x - x0];
    p = hd::addLight(p, c, x, y);
  }
}

void compose(int x0, int y0, int w, int h, uint16_t* out) {
  for (int rr = 0; rr < h; rr++) {
    const int y = y0 + rr;
    uint16_t* row = out + rr * w;
    if (y < HORIZON) {
      for (int x = x0; x < x0 + w; x++) {
        int r, g, b;
        skyRgb(x, y, r, g, b);
        row[x - x0] = hd::addRgb(0, r, g, b, x, y);
      }
    } else if (y == HORIZON) {
      for (int x = x0; x < x0 + w; x++) row[x - x0] = hd::addRgb(0, 12, 33, 38, x, y);
    } else if (y < LAKE_END) {
      const int d = y - HORIZON;
      const int src = HORIZON - 1 - d, fade = lakeFade[d], shift = lakeShift[d];
      for (int x = x0; x < x0 + w; x++) {
        int xs = x + shift;
        xs = xs < 0 ? 0 : (xs >= SCREEN_W ? SCREEN_W - 1 : xs);
        int r, g, b;
        skyRgb(xs, src, r, g, b);
        row[x - x0] = hd::addRgb(0, 1 + (r * fade >> 8), 3 + (g * fade >> 8), 8 + (b * fade >> 8), x, y);
      }
    } else {
      for (int x = x0; x < x0 + w; x++) row[x - x0] = hd::addRgb(0, 1, 3, 8, x, y);
    }
  }
  // gwiazdy (migotanie)
  for (const auto& s : STARS)
    if (s.x >= x0 && s.x < x0 + w && s.y >= y0 && s.y < y0 + h) {
      const int tw = 160 + int(95 * sinf((s.phase + frameNo * 3) * (2 * float(M_PI) / 256)));
      const uint8_t v = uint8_t(s.b * tw >> 8);
      const uint8_t c[3] = {v, v, v};
      uint16_t& p = out[(s.y - y0) * w + s.x - x0];
      p = hd::addLight(p, c, s.x, s.y);
    }
  for (const auto& m : meteors)
    if (m.on) drawMeteor(m, x0, y0, w, h, out);
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

// ---------------------------------------------------------------- ruch
void updateColumns() {
  struct Curtain {
    float yb, amp, h0, ph, gain;
  };
  static const Curtain C[CURTAINS] = {{150, 26, 95, 0.0f, 1.0f}, {118, 18, 70, 2.1f, 0.6f}};
  float boost = 1.0f;
  if (brightFrames) {
    boost = 1.0f + 0.7f * (brightFrames > 60 ? 1.0f : brightFrames / 60.0f);
    brightFrames--;
  }
  for (int k = 0; k < CURTAINS; k++) {
    const Curtain& c = C[k];
    const int shift = int(frameNo * (1 + k)) / 2;
    for (int x = 0; x < SCREEN_W; x++) {
      const float fx = float(x);
      base[k][x] = int16_t(c.yb + c.amp * sinf(fx / 85 + t + c.ph) + 10 * sinf(fx / 31 - 1.7f * t + c.ph) +
                           4 * sinf(fx / 9 + 3 * t));
      const float hgt = c.h0 + 30 * sinf(fx / 57 - t + c.ph);
      invH[k][x] = uint16_t(64 * 256 / hgt);
      const float n = RAYS[(x + shift) % SCREEN_W] / 255.0f;
      float r = 0.55f + 0.45f * sinf(fx * 0.35f + 6 * n) * sinf(fx * 0.07f - 2 * t + c.ph);
      r = r < 0.1f ? 0.1f : (r > 1 ? 1 : r);
      r *= 0.7f + 0.3f * sinf(fx / 40 + 2 * t + k);
      ray[k][x] = uint16_t(r * c.gain * boost * 256);
    }
  }
  for (int d = 0; d <= LAKE_ROWS; d++)
    lakeShift[d] = int8_t(lroundf(1.2f * sinf((HORIZON + d) * 0.45f + t * 3) * (0.2f + d / 80.0f)));
}

void stepMeteors() {
  if (meteorQueue > 0 && frameNo % 8 == 0)
    for (auto& m : meteors)
      if (!m.on) {
        const bool left = rnd(2);
        m = {true, float(80 + rnd(320)), float(10 + rnd(40)), (left ? -1.0f : 1.0f) * (7 + rnd(4)), 3.0f + rnd(3)};
        meteorQueue--;
        break;
      }
  for (auto& m : meteors) {
    if (!m.on) continue;
    m.x += m.vx;
    m.y += m.vy;
    if (m.x < -40 || m.x > SCREEN_W + 40 || m.y > HORIZON + 40) m.on = false;
  }
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
    if (lastMinute >= 0 && now->tm_min != lastMinute) meteorQueue += 1;
    if (lastHour >= 0 && now->tm_hour != lastHour) {
      meteorQueue += 2;
      brightFrames = 120;
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
  for (int u = 0; u < 256; u++) {
    const float f = u / 64.0f;
    lutUp[u] = uint16_t((expf(-2.2f * f) + 0.35f * expf(-1.1f * f)) * 0.95f * 256);
  }
  for (int v = 0; v < 64; v++) {
    const float f = v / 64.0f;
    lutDown[v] = uint16_t((expf(-14 * f) + 0.35f * expf(-4 * f)) * 0.95f * 256);
  }
  for (int u = 0; u <= 64; u++) {
    const float fr = u / 64.0f;
    aurCol[u][0] = uint8_t(40 * (1 - fr) + 170 * fr);
    aurCol[u][1] = uint8_t(255 * (1 - fr) + 50 * fr);
    aurCol[u][2] = uint8_t(130 * (1 - fr) + 255 * fr);
  }
  for (int y = 0; y < HORIZON; y++) {
    const float f = float(y) / HORIZON;
    skyRow[y][0] = uint8_t(1 * (1 - f) + 6 * f);
    skyRow[y][1] = uint8_t(3 * (1 - f) + 16 * f);
    skyRow[y][2] = uint8_t(10 * (1 - f) + 34 * f);
  }
  for (int d = 0; d <= LAKE_ROWS; d++) {
    float fade = 0.55f - d / 400.0f;
    if (LAKE_ROWS - d < 25) fade *= (LAKE_ROWS - d) / 25.0f;
    lakeFade[d] = uint8_t(fade * 256);
  }
  updateColumns();
}

void drawAll() {
  tiles.markAll();
  tiles.flush(compose);
}

void frame(uint32_t nowMs, const struct tm* now) {
  frameNo++;
  updateClock(now);
  t += 2 * float(M_PI) / 200;
  if (t > 2 * float(M_PI) * 100) t -= 2 * float(M_PI) * 100;
  updateColumns();
  stepMeteors();
  tiles.mark(0, ANIM_TOP, SCREEN_W, LAKE_END - ANIM_TOP);
  tiles.flush(compose);
}

}  // namespace face
