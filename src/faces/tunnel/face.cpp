// Tarcza "tunnel" (cyberpunk): neonowy tunel pedzacy na widza - ramki
// magenta/cyan wylaniaja sie zza tablicy z godzina i rosna ku krawedziom,
// od srodka wystrzeliwuja smugi swiatla. Cyfry cyan w tablicy z zolta ramka,
// data magenta pod spodem. Zmiana minuty = przyspieszenie (warp), pelna
// godzina = dlugi warp.
//
// Ekran dzielony na kafelki 16x8; co klatke oznaczane sa kafelki starej i
// nowej geometrii (ramki, smugi), wysylane tylko one - scena skladana od
// zera w compose().

#include "core/face.h"

#include <esp_random.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "assets_gen.h"
#include "core/pixel.h"
#include "core/push.h"

using namespace assets;

namespace face {
namespace {

constexpr int CX = 240, CY = 150;
constexpr int TILE_W = 16, TILE_H = 8;
constexpr int TILES_X = SCREEN_W / TILE_W, TILES_Y = SCREEN_H / TILE_H;
uint32_t dirty[TILES_Y];

// wnetrze tablicy jest nieprzezroczyste - zmiany tunelu za nim nic nie zmieniaja
constexpr int OPAQUE_X0 = PLATE_X + 8 + 14, OPAQUE_X1 = PLATE_X + PLATE_W - 8 - 14;
constexpr int OPAQUE_Y0 = PLATE_Y + 8, OPAQUE_Y1 = PLATE_Y + PLATE_H - 8;

constexpr int RINGS = 12;
constexpr float RING_STEP = 0.55f;
constexpr int STREAKS = 14;
constexpr int DATE_TOP_Y = DATE_BASELINE - 22, DATE_BOTTOM_Y = DATE_BASELINE + 10;

const uint8_t MAGENTA[3] = {255, 40, 200}, CYAN[3] = {0, 240, 255};
const uint8_t STREAK_COLORS[4][3] = {{252, 238, 10}, {255, 0, 60}, {255, 255, 255}, {0, 240, 255}};
const uint8_t DIAG[3] = {90, 40, 140};
const uint8_t HALO[3] = {115, 45, 18};  // swiecenie ramki: 1.0 rdzen, 0.45 / 0.18 poswiata (x/256)

struct Ring {
  int16_t hx, hy;  // polowa szerokosci/wysokosci
  uint8_t t;       // grubosc rdzenia
  uint8_t col[3][3];
};
Ring rings[RINGS];
int ringCount = 0;

struct Streak {
  float d, speed, ca, sa;
  uint8_t color;
};
struct Segment {
  int16_t x0, y0, x1, y1;
  uint8_t color;
  bool on;
};
Streak streaks[STREAKS];
Segment segs[STREAKS];

float phase = 0;
uint32_t ringBase = 0;  // numer ramki k=0 (do koloru)
int warp = 0;  // klatki przyspieszenia

int8_t digits[4] = {-1, -1, -1, -1};
bool colonOn = false;
struct DateChar {
  int16_t x;
  int8_t glyph;
};
DateChar dateChars[40];
int dateLen = 0;
char dateShown[48] = "";
int lastMinute = -1, lastHour = -1;

uint32_t rnd(uint32_t n) { return esp_random() % n; }
float frnd() { return (esp_random() & 0xFFFF) / 65536.0f; }

// ---------------------------------------------------------------- swiatlo
inline uint16_t addLight(uint16_t p, const uint8_t a[3], int x, int y) {
  const uint8_t t = px::BAYER4[y & 3][x & 3];
  int r = (p >> 11) + ((a[0] + (t >> 1)) >> 3);
  int g = ((p >> 5) & 63) + ((a[1] + (t >> 2)) >> 2);
  int b = (p & 31) + ((a[2] + (t >> 1)) >> 3);
  if (r > 31) r = 31;
  if (g > 63) g = 63;
  if (b > 31) b = 31;
  return uint16_t((r << 11) | (g << 5) | b);
}

void drawNeon(const uint8_t* data, int gw, int gh, int gx, int gy, const uint8_t (*lut)[3], int x0, int y0, int w,
              int h, uint16_t* out) {
  const int ya = gy > y0 ? gy : y0, yb = (gy + gh) < (y0 + h) ? (gy + gh) : (y0 + h);
  const int xa = gx > x0 ? gx : x0, xb = (gx + gw) < (x0 + w) ? (gx + gw) : (x0 + w);
  for (int y = ya; y < yb; y++) {
    const uint8_t* src = data + (y - gy) * gw;
    uint16_t* dst = out + (y - y0) * w;
    for (int x = xa; x < xb; x++) {
      const uint8_t idx = src[x - gx];
      if (idx) dst[x - x0] = addLight(dst[x - x0], lut[idx], x, y);
    }
  }
}

// prostokat swiatla przyciety do kawalka
void lightRect(int rx, int ry, int rw, int rh, const uint8_t c[3], int x0, int y0, int w, int h, uint16_t* out) {
  int xa = rx > x0 ? rx : x0, xb = (rx + rw) < (x0 + w) ? (rx + rw) : (x0 + w);
  int ya = ry > y0 ? ry : y0, yb = (ry + rh) < (y0 + h) ? (ry + rh) : (y0 + h);
  for (int y = ya; y < yb; y++)
    for (int x = xa; x < xb; x++) out[(y - y0) * w + x - x0] = addLight(out[(y - y0) * w + x - x0], c, x, y);
}

void drawRing(const Ring& r, int x0, int y0, int w, int h, uint16_t* out) {
  const int L = CX - r.hx, R = CX + r.hx, T = CY - r.hy, B = CY + r.hy;
  const int s = r.t / 2;
  // pasy: poswiata 2, poswiata 1, rdzen, poswiata 1, poswiata 2
  const int off[5] = {-s - 2, -s - 1, -s, -s + r.t, -s + r.t + 1};
  const int len[5] = {1, 1, r.t, 1, 1};
  const uint8_t lvl[5] = {2, 1, 0, 1, 2};
  for (int i = 0; i < 5; i++) {
    const uint8_t* c = r.col[lvl[i]];
    lightRect(L, T + off[i], R - L + 1, len[i], c, x0, y0, w, h, out);
    lightRect(L, B + off[i], R - L + 1, len[i], c, x0, y0, w, h, out);
    lightRect(L + off[i], T + s + 2, len[i], B - T - 2 * s - 3, c, x0, y0, w, h, out);
    lightRect(R + off[i], T + s + 2, len[i], B - T - 2 * s - 3, c, x0, y0, w, h, out);
  }
}

void drawSegment(const Segment& g, int x0, int y0, int w, int h, uint16_t* out) {
  const int bx0 = (g.x0 < g.x1 ? g.x0 : g.x1) - 1, bx1 = (g.x0 > g.x1 ? g.x0 : g.x1) + 1;
  const int by0 = (g.y0 < g.y1 ? g.y0 : g.y1) - 1, by1 = (g.y0 > g.y1 ? g.y0 : g.y1) + 1;
  if (bx1 < x0 || bx0 >= x0 + w || by1 < y0 || by0 >= y0 + h) return;
  const int dx = g.x1 - g.x0, dy = g.y1 - g.y0;
  const int n = (abs(dx) > abs(dy) ? abs(dx) : abs(dy)) + 1;
  const bool flat = abs(dx) > abs(dy);
  const uint8_t* base = STREAK_COLORS[g.color];
  for (int i = 0; i < n; i++) {
    const int x = g.x0 + dx * i / n, y = g.y0 + dy * i / n;
    // ogon ciemny, glowa jasna
    const int f = 40 + 216 * i / n;
    const uint8_t c[3] = {uint8_t(base[0] * f >> 8), uint8_t(base[1] * f >> 8), uint8_t(base[2] * f >> 8)};
    for (int k = 0; k < 2; k++) {
      const int px_ = flat ? x : x + k, py_ = flat ? y + k : y;
      if (px_ >= x0 && px_ < x0 + w && py_ >= y0 && py_ < y0 + h)
        out[(py_ - y0) * w + px_ - x0] = addLight(out[(py_ - y0) * w + px_ - x0], c, px_, py_);
    }
  }
}

// cztery przekatne z rogow ekranu do punktu zbiegu
void drawDiagonals(int x0, int y0, int w, int h, uint16_t* out) {
  for (int y = y0; y < y0 + h; y++) {
    const int d = y <= CY ? y * CX / CY : (SCREEN_H - y) * CX / (SCREEN_H - CY);
    const int xs[2] = {d, SCREEN_W - 1 - d};
    for (int x : xs)
      for (int k = 0; k < 2; k++)
        if (x + k >= x0 && x + k < x0 + w) out[(y - y0) * w + x + k - x0] = addLight(out[(y - y0) * w + x + k - x0], DIAG, x + k, y);
  }
}

void drawPlate(int x0, int y0, int w, int h, uint16_t* out) {
  if (x0 >= PLATE_X + PLATE_W || x0 + w <= PLATE_X || y0 >= PLATE_Y + PLATE_H || y0 + h <= PLATE_Y) return;
  const int xa = PLATE_X > x0 ? PLATE_X : x0, xb = (PLATE_X + PLATE_W) < (x0 + w) ? (PLATE_X + PLATE_W) : (x0 + w);
  const int ya = PLATE_Y > y0 ? PLATE_Y : y0, yb = (PLATE_Y + PLATE_H) < (y0 + h) ? (PLATE_Y + PLATE_H) : (y0 + h);
  for (int y = ya; y < yb; y++)
    for (int x = xa; x < xb; x++)
      if (PLATE_FILL[(y - PLATE_Y) * PLATE_W + x - PLATE_X]) out[(y - y0) * w + x - x0] = px::dither(4, 2, 10, x, y);
  drawNeon(PLATE_GLOW, PLATE_W, PLATE_H, PLATE_X, PLATE_Y, LUT_YELLOW, x0, y0, w, h, out);
  if (y0 < DIGIT_TOP + DIGIT_CELL_H && y0 + h > DIGIT_TOP) {
    for (int i = 0; i < 4; i++)
      if (digits[i] >= 0)
        drawNeon(DIGIT_DATA[digits[i]], DIGIT_CELL_W, DIGIT_CELL_H, DIGIT_X[i], DIGIT_TOP, LUT_CYAN, x0, y0, w, h, out);
    if (colonOn) drawNeon(COLON_DATA, COLON_W, DIGIT_CELL_H, COLON_X, DIGIT_TOP, LUT_CYAN, x0, y0, w, h, out);
  }
}

void compose(int x0, int y0, int w, int h, uint16_t* out) {
  memset(out, 0, size_t(w) * h * 2);
  drawDiagonals(x0, y0, w, h, out);
  for (int i = 0; i < ringCount; i++) drawRing(rings[i], x0, y0, w, h, out);
  for (const auto& g : segs)
    if (g.on) drawSegment(g, x0, y0, w, h, out);
  drawPlate(x0, y0, w, h, out);
  if (y0 < DATE_BOTTOM_Y && y0 + h > DATE_TOP_Y)
    for (int i = 0; i < dateLen; i++) {
      if (dateChars[i].glyph < 0) continue;
      const DateGlyph& g = DATE_GLYPHS[dateChars[i].glyph];
      if (g.w)
        drawNeon(DATE_DATA + g.offset, g.w, g.h, dateChars[i].x + g.xoff, DATE_BASELINE + g.yoff, LUT_MAGENTA, x0, y0,
                 w, h, out);
    }
  for (int i = 0; i < w * h; i++) out[i] = px::swap(out[i]);
}

// ---------------------------------------------------------------- kafelki
void mark(int x, int y, int w, int h, bool skipOpaque = true) {
  int tx0 = x / TILE_W, tx1 = (x + w - 1) / TILE_W, ty0 = y / TILE_H, ty1 = (y + h - 1) / TILE_H;
  if (x < 0) tx0 = 0;
  if (y < 0) ty0 = 0;
  if (x + w <= 0 || y + h <= 0 || x >= SCREEN_W || y >= SCREEN_H) return;
  if (tx1 >= TILES_X) tx1 = TILES_X - 1;
  if (ty1 >= TILES_Y) ty1 = TILES_Y - 1;
  for (int ty = ty0; ty <= ty1; ty++)
    for (int tx = tx0; tx <= tx1; tx++) {
      if (skipOpaque && tx * TILE_W >= OPAQUE_X0 && (tx + 1) * TILE_W <= OPAQUE_X1 && ty * TILE_H >= OPAQUE_Y0 &&
          (ty + 1) * TILE_H <= OPAQUE_Y1)
        continue;
      dirty[ty] |= 1u << tx;
    }
}

void markRing(const Ring& r) {
  const int L = CX - r.hx, R = CX + r.hx, T = CY - r.hy, B = CY + r.hy, p = r.t / 2 + 3;
  mark(L - p, T - p, R - L + 2 * p + 1, 2 * p + 1);
  mark(L - p, B - p, R - L + 2 * p + 1, 2 * p + 1);
  mark(L - p, T - p, 2 * p + 1, B - T + 2 * p + 1);
  mark(R - p, T - p, 2 * p + 1, B - T + 2 * p + 1);
}

void markSegment(const Segment& g) {
  if (!g.on) return;
  const int dx = g.x1 - g.x0, dy = g.y1 - g.y0;
  const int n = (abs(dx) > abs(dy) ? abs(dx) : abs(dy)) / 4 + 1;
  for (int i = 0; i <= n; i++) mark(g.x0 + dx * i / n - 1, g.y0 + dy * i / n - 1, 4, 4);
}

void flush() {
  for (int ty = 0; ty < TILES_Y; ty++) {
    uint32_t bits = dirty[ty];
    dirty[ty] = 0;
    int tx = 0;
    while (bits) {
      while (!(bits & 1)) {
        bits >>= 1;
        tx++;
      }
      int run = 0;
      while (bits & 1) {
        bits >>= 1;
        run++;
      }
      pushComposed(tx * TILE_W, ty * TILE_H, run * TILE_W, TILE_H, compose);
      tx += run;
    }
  }
}

// ---------------------------------------------------------------- ruch
void buildRings() {
  ringCount = 0;
  for (int k = 0; k < RINGS; k++) {
    const float z = (k + 1 - phase) * RING_STEP;
    if (z < 0.05f) continue;
    const int hx = int(300 / z), hy = int(200 / z);
    if (hx > CX + 10 && hy > SCREEN_H - CY + 10) continue;  // juz za ekranem
    if (hx < OPAQUE_X1 - CX - 6 && hy < CY - OPAQUE_Y0 - 6) continue;  // cala za tablica
    Ring& r = rings[ringCount++];
    r.hx = hx;
    r.hy = hy;
    r.t = z < 1.6f ? 3 : (z < 2.6f ? 2 : 1);
    float b = 0.25f + 0.9f / z;
    if (b > 1) b = 1;
    // kolor przypisany do ramki (nie do pozycji), zeby nie migal przy zawijaniu
    const uint32_t id = k + ringBase;
    const uint8_t* c = (id & 1) ? CYAN : MAGENTA;
    for (int l = 0; l < 3; l++)
      for (int ch = 0; ch < 3; ch++) r.col[l][ch] = uint8_t(c[ch] * b * HALO[l] / 115);
  }
}

void respawnStreak(Streak& s, bool anywhere) {
  const float a = frnd() * 2 * float(M_PI);
  s.ca = cosf(a);
  s.sa = sinf(a) * 0.7f;
  s.d = anywhere ? frnd() : 0;
  s.speed = 0.010f + frnd() * 0.014f;
  s.color = rnd(4);
}

void buildSegments() {
  const float boost = warp ? 3.0f : 1.0f;
  for (int i = 0; i < STREAKS; i++) {
    Streak& s = streaks[i];
    s.d += s.speed * boost;
    const float tail = s.d - 0.15f;
    const float r1 = 30 + s.d * s.d * 380, r0 = 30 + (tail > 0 ? tail * tail : 0) * 380;
    if (r0 > 420) {
      respawnStreak(s, false);
      segs[i].on = false;
      continue;
    }
    segs[i] = {int16_t(CX + s.ca * r0), int16_t(CY + s.sa * r0), int16_t(CX + s.ca * r1), int16_t(CY + s.sa * r1),
               s.color, true};
  }
}

void setDate(const char* utf8) {
  int cps[40], n = 0, width = 0;
  for (const char* p = utf8; *p && n < 40;) {
    const uint8_t c = *p++;
    int cp = c;
    if ((c & 0xE0) == 0xC0) cp = ((c & 0x1F) << 6) | (*p++ & 0x3F);
    else if ((c & 0xF0) == 0xE0) {
      cp = ((c & 0x0F) << 12) | ((p[0] & 0x3F) << 6) | (p[1] & 0x3F);
      p += 2;
    }
    cps[n++] = cp;
  }
  for (int i = 0; i < n; i++) {
    dateChars[i].glyph = -1;
    for (int g = 0; g < DATE_GLYPH_COUNT; g++)
      if (DATE_GLYPHS[g].cp == cps[i]) dateChars[i].glyph = g;
    if (dateChars[i].glyph >= 0) width += DATE_GLYPHS[dateChars[i].glyph].adv;
  }
  int x = DATE_CENTER_X - width / 2;
  for (int i = 0; i < n; i++) {
    dateChars[i].x = x;
    if (dateChars[i].glyph >= 0) x += DATE_GLYPHS[dateChars[i].glyph].adv;
  }
  dateLen = n;
}

void updateClock(const struct tm* now) {
  int8_t nd[4] = {-1, -1, -1, -1};
  bool colon = false;
  char date[48] = "";
  if (now) {
    nd[0] = now->tm_hour / 10;
    nd[1] = now->tm_hour % 10;
    nd[2] = now->tm_min / 10;
    nd[3] = now->tm_min % 10;
    colon = (now->tm_sec & 1) == 0;
    snprintf(date, sizeof(date), "%s %02d.%02d.%04d", WEEKDAYS[now->tm_wday], now->tm_mday, now->tm_mon + 1,
             now->tm_year + 1900);
    if (lastMinute >= 0 && now->tm_min != lastMinute) warp = 25;
    if (lastHour >= 0 && now->tm_hour != lastHour) warp = 80;
    lastMinute = now->tm_min;
    lastHour = now->tm_hour;
  }
  for (int i = 0; i < 4; i++)
    if (nd[i] != digits[i]) {
      digits[i] = nd[i];
      mark(DIGIT_X[i], DIGIT_TOP, DIGIT_CELL_W, DIGIT_CELL_H, false);
    }
  if (colon != colonOn) {
    colonOn = colon;
    mark(COLON_X, DIGIT_TOP, COLON_W, DIGIT_CELL_H, false);
  }
  if (strcmp(date, dateShown) != 0) {
    strcpy(dateShown, date);
    setDate(date);
    mark(0, DATE_TOP_Y, SCREEN_W, DATE_BOTTOM_Y - DATE_TOP_Y, false);
  }
}

}  // namespace

void begin() {
  for (auto& s : streaks) respawnStreak(s, true);
  buildRings();
  buildSegments();
}

void drawAll() {
  for (auto& d : dirty) d = (1u << TILES_X) - 1;
  flush();
}

void frame(uint32_t nowMs, const struct tm* now) {
  updateClock(now);
  // stara geometria
  for (int i = 0; i < ringCount; i++) markRing(rings[i]);
  for (const auto& g : segs) markSegment(g);
  // ruch
  phase += warp ? 0.10f : 0.03f;
  if (warp) warp--;
  while (phase >= 1) {
    phase -= 1;
    ringBase++;
  }
  buildRings();
  buildSegments();
  for (int i = 0; i < ringCount; i++) markRing(rings[i]);
  for (const auto& g : segs) markSegment(g);
  flush();
}

}  // namespace face
