#include "scene.h"

#include <math.h>
#include <string.h>
#include "core/pixel.h"

using namespace assets;

constexpr int FOG_TOP = 200;

namespace scene {

State st;

namespace {

struct DateChar {
  int16_t x;
  int8_t glyph;
};
DateChar dateChars[MAX_DATE_CHARS];
int dateLen = 0;
DateChar tempChars[8];
int tempLen = 0;

const uint16_t RAIN_COLOR = px::rgb565(170, 195, 230);
constexpr uint8_t RAIN_ALPHA = 140;
const uint16_t SNOW_COLOR = px::rgb565(235, 240, 255);

// Swiatlo neonu dodawane do tla z nasyceniem, z ditheringiem Bayera
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

int decodeUtf8(const char*& s) {
  uint8_t c = *s++;
  if (c < 0x80) return c;
  if ((c & 0xE0) == 0xC0) return ((c & 0x1F) << 6) | (*s++ & 0x3F);
  if ((c & 0xF0) == 0xE0) {
    int v = ((c & 0x0F) << 12) | ((s[0] & 0x3F) << 6) | (s[1] & 0x3F);
    s += 2;
    return v;
  }
  return '?';
}

int findDateGlyph(int cp) {
  for (int i = 0; i < DATE_GLYPH_COUNT; i++)
    if (DATE_GLYPHS[i].cp == cp) return i;
  return -1;
}

// Nakladanie glifu-neonu (index -> LUT RGB888) przycietego do prostokata
void drawNeon(const uint8_t* data, int gw, int gh, int gx, int gy, const uint8_t (*lut)[3],
              int x0, int y0, int w, int h, uint16_t* out) {
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

// Falowanie odbic: przesuniecie wiersza ulicy w poziomie
inline int rippleShift(int y) {
  static int8_t table[64];
  static bool ready = false;
  if (!ready) {
    for (int i = 0; i < 64; i++) table[i] = int8_t(lroundf(2.5f * sinf(i * 2 * M_PI / 64)));
    ready = true;
  }
  return table[(y * 9 + st.ripple * 4) & 63];
}

}  // namespace

void setDate(const char* utf8) {
  int cps[MAX_DATE_CHARS], n = 0, width = 0;
  for (const char* p = utf8; *p && n < MAX_DATE_CHARS;) {
    cps[n] = decodeUtf8(p);
    const int g = findDateGlyph(cps[n]);
    width += g >= 0 ? DATE_GLYPHS[g].adv : 0;
    n++;
  }
  int x = DATE_CENTER_X - width / 2;
  for (int i = 0; i < n; i++) {
    const int g = findDateGlyph(cps[i]);
    dateChars[i] = {int16_t(x), int8_t(g)};
    if (g >= 0) x += DATE_GLYPHS[g].adv;
  }
  dateLen = n;
}

void setTemp(const char* utf8) {
  int cps[8], n = 0, width = 0;
  for (const char* p = utf8; *p && n < 8;) {
    cps[n] = decodeUtf8(p);
    for (int g = 0; g < WX_TEMP_GLYPH_COUNT; g++)
      if (WX_TEMP_GLYPHS[g].cp == cps[n]) width += WX_TEMP_GLYPHS[g].adv;
    n++;
  }
  int x = WX_TEMP_CENTER_X - width / 2;
  for (int i = 0; i < n; i++) {
    int8_t gi = -1;
    for (int g = 0; g < WX_TEMP_GLYPH_COUNT; g++)
      if (WX_TEMP_GLYPHS[g].cp == cps[i]) gi = g;
    tempChars[i] = {int16_t(x), gi};
    if (gi >= 0) x += WX_TEMP_GLYPHS[gi].adv;
  }
  tempLen = n;
}

void dropBounds(const Drop& d, int& x0, int& y0, int& x1, int& y1) {
  x1 = int(d.x) + 2;
  x0 = int(d.x - d.len * st.slope) - 1;
  y0 = int(d.y);
  y1 = int(d.y) + d.len + 1;
}

void compose(int x0, int y0, int w, int h, uint16_t* out) {
  // 1. tlo; ulica z falujacym odbiciem
  for (int r = 0; r < h; r++) {
    const int y = y0 + r;
    const uint16_t* src = BG + y * SCREEN_W;
    uint16_t* dst = out + r * w;
    if (y > STREET_Y) {
      const int s = rippleShift(y);
      for (int i = 0; i < w; i++) dst[i] = src[(x0 + i + s + SCREEN_W) % SCREEN_W];
    } else {
      memcpy(dst, src + x0, w * 2);
    }
  }

  // 1b. mgla nad ulica (gdy sucho)
  if (st.fog && y0 + h > FOG_TOP && y0 <= STREET_Y) {
    for (int y = (y0 > FOG_TOP ? y0 : FOG_TOP); y < y0 + h && y <= STREET_Y; y++) {
      const float f = float(y - FOG_TOP) / (STREET_Y - FOG_TOP);
      const float k = f * sqrtf(f) * 0.6f;
      const uint8_t c[3] = {uint8_t(30 * k), uint8_t(20 * k), uint8_t(45 * k)};
      uint16_t* row = out + (y - y0) * w;
      for (int i = 0; i < w; i++) row[i] = addLight(row[i], c, x0 + i, y);
    }
  }

  // 2. zgaszone okna
  for (int i = 0; i < WINDOW_COUNT; i++) {
    if (!st.windowDark[i]) continue;
    const Window& wd = WINDOWS[i];
    for (int y = wd.y; y < wd.y + WINDOW_H; y++)
      for (int x = wd.x; x < wd.x + WINDOW_W; x++)
        if (x >= x0 && x < x0 + w && y >= y0 && y < y0 + h) out[(y - y0) * w + x - x0] = wd.dark;
  }

  // 3. pas skanowania hologramu
  if (st.holoBand >= 0) {
    static const uint8_t SCAN[3] = {0, 70, 70};
    for (int y = st.holoBand; y < st.holoBand + 3 && y < HOLO_Y1; y++)
      for (int x = HOLO_X0; x < HOLO_X1; x++)
        if (x >= x0 && x < x0 + w && y >= y0 && y < y0 + h)
          out[(y - y0) * w + x - x0] = addLight(out[(y - y0) * w + x - x0], SCAN, x, y);
  }

  // 3b. szyld pogody: ikona + temperatura
  if (y0 < HOLO_Y1 && y0 + h > HOLO_Y0 && x0 < HOLO_X1 && x0 + w > HOLO_X0) {
    if (st.wxIcon >= 0)
      drawNeon(WX_ICONS[st.wxIcon], WX_ICON_SIZE, WX_ICON_SIZE, WX_ICON_X, WX_ICON_Y, LUT_WX, x0, y0, w, h, out);
    for (int i = 0; i < tempLen; i++) {
      if (tempChars[i].glyph < 0) continue;
      const DateGlyph& g = WX_TEMP_GLYPHS[tempChars[i].glyph];
      drawNeon(WX_TEMP_DATA + g.offset, g.w, g.h, tempChars[i].x + g.xoff, WX_TEMP_BASELINE + g.yoff, LUT_WX, x0, y0,
               w, h, out);
    }
  }

  // 4. latajace auto
  if (st.carX > -CAR_W && st.carX < SCREEN_W) {
    for (int y = CAR_Y; y < CAR_Y + CAR_H; y++) {
      if (y < y0 || y >= y0 + h) continue;
      for (int x = st.carX; x < st.carX + CAR_W; x++) {
        if (x < x0 || x >= x0 + w) continue;
        const int i = (y - CAR_Y) * CAR_W + (x - st.carX);
        if (CAR_ALPHA[i]) {
          uint16_t& p = out[(y - y0) * w + x - x0];
          p = px::blend(p, CAR_COLOR[i], CAR_ALPHA[i]);
        }
      }
    }
  }

  // 5. szyld: cyfry i dwukropek
  if (y0 < DIGIT_TOP + DIGIT_CELL_H && y0 + h > DIGIT_TOP) {
    drawNeon(COLON_DATA, COLON_W, DIGIT_CELL_H, COLON_X, DIGIT_TOP, LUT_ON, x0, y0, w, h, out);
    for (int i = 0; i < 4; i++) {
      const Slot& s = st.slots[i];
      if (s.digit < 0 || s.lit == Lit::Off) continue;
      drawNeon(DIGIT_DATA[s.digit], DIGIT_CELL_W, DIGIT_CELL_H, DIGIT_X[i], DIGIT_TOP,
               s.lit == Lit::On ? LUT_ON : LUT_DIM, x0, y0, w, h, out);
    }
  }

  // 6. data
  if (y0 < DATE_BAND_BOTTOM && y0 + h > DATE_BAND_TOP) {
    for (int i = 0; i < dateLen; i++) {
      if (dateChars[i].glyph < 0) continue;
      const DateGlyph& g = DATE_GLYPHS[dateChars[i].glyph];
      if (g.w)
        drawNeon(DATE_DATA + g.offset, g.w, g.h, dateChars[i].x + g.xoff, DATE_BASELINE + g.yoff,
                 LUT_DATE, x0, y0, w, h, out);
    }
  }

  // 7. deszcz albo snieg (na wierzchu wszystkiego)
  for (int i = 0; i < st.dropCount; i++) {
    const Drop& d = st.drops[i];
    int bx0, by0, bx1, by1;
    dropBounds(d, bx0, by0, bx1, by1);
    if (bx1 < x0 || bx0 >= x0 + w || by1 < y0 || by0 >= y0 + h) continue;
    if (st.snow) {
      for (int yy = int(d.y); yy < int(d.y) + 2; yy++)
        for (int xx = int(d.x); xx < int(d.x) + 2; xx++)
          if (xx >= x0 && xx < x0 + w && yy >= y0 && yy < y0 + h) out[(yy - y0) * w + xx - x0] = SNOW_COLOR;
      continue;
    }
    for (int k = 0; k < d.len; k++) {
      const int x = int(d.x - k * st.slope), y = int(d.y) + k;
      if (x >= x0 && x < x0 + w && y >= y0 && y < y0 + h) {
        uint16_t& p = out[(y - y0) * w + x - x0];
        p = px::blend(p, RAIN_COLOR, RAIN_ALPHA);
      }
    }
  }

  // 7b. piorun (tylko w trakcie blysku)
  if (st.flash && st.bolt) {
    static const uint8_t BOLT[3] = {200, 210, 255};
    for (int k = 0; k + 1 < BOLT_POINTS; k++) {
      const int ya = k * 18, yb = (k + 1) * 18;
      for (int y = ya; y < yb; y++) {
        const int x = st.boltX[k] + (st.boltX[k + 1] - st.boltX[k]) * (y - ya) / 18;
        for (int dx = -1; dx <= 1; dx++)
          if (x + dx >= x0 && x + dx < x0 + w && y >= y0 && y < y0 + h)
            out[(y - y0) * w + x + dx - x0] = addLight(out[(y - y0) * w + x + dx - x0], BOLT, x + dx, y);
      }
    }
  }

  // 8. blysk burzy
  if (st.flash) {
    const uint8_t f[3] = {uint8_t(st.flash), uint8_t(st.flash), uint8_t(st.flash + st.flash / 4)};
    for (int r = 0; r < h; r++)
      for (int i = 0; i < w; i++) out[r * w + i] = addLight(out[r * w + i], f, x0 + i, y0 + r);
  }

  for (int i = 0; i < w * h; i++) out[i] = px::swap(out[i]);
}

}  // namespace scene
