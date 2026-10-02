#include "scene.h"

#include <string.h>
#include "core/pixel.h"

using namespace assets;

namespace scene {

State st;

namespace {

constexpr int MAX_DATE_CHARS = 32;
struct DateChar {
  int16_t x;
  int8_t glyph;
};
DateChar dateChars[MAX_DATE_CHARS];
int dateLen = 0;

const uint16_t RAIN_COLOR = px::rgb565(150, 155, 172);
constexpr uint8_t RAIN_ALPHA = 70;

inline uint16_t addLight(uint16_t p, const uint8_t a[3], int x, int y) {
  const uint8_t t = px::BAYER4[y & 3][x & 3];
  int r = (p >> 11) + ((a[0] + (t >> 1)) >> 3);
  int g = ((p >> 5) & 63) + ((a[1] + (t >> 2)) >> 2);
  int b = (p & 31) + ((a[2] + (t >> 1)) >> 3);
  return uint16_t(((r > 31 ? 31 : r) << 11) | ((g > 63 ? 63 : g) << 5) | (b > 31 ? 31 : b));
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

void drawLight(const uint8_t* data, int gw, int gh, int gx, int gy, const uint8_t (*lut)[3], int x0,
               int y0, int w, int h, uint16_t* out) {
  const int ya = gy > y0 ? gy : y0, yb = (gy + gh) < (y0 + h) ? (gy + gh) : (y0 + h);
  const int xa = gx > x0 ? gx : x0, xb = (gx + gw) < (x0 + w) ? (gx + gw) : (x0 + w);
  for (int y = ya; y < yb; y++) {
    const uint8_t* src = data + (y - gy) * gw;
    uint16_t* dst = out + (y - y0) * w;
    for (int x = xa; x < xb; x++)
      if (src[x - gx]) dst[x - x0] = addLight(dst[x - x0], lut[src[x - gx]], x, y);
  }
}

// Kropla na szybie: rozjasnienie malejace od srodka
void drawBlob(int cx, int cy, int r, int k, int x0, int y0, int w, int h, uint16_t* out) {
  for (int y = cy - r; y <= cy + r; y++) {
    if (y < y0 || y >= y0 + h) continue;
    for (int x = cx - r; x <= cx + r; x++) {
      if (x < x0 || x >= x0 + w) continue;
      const int d2 = (x - cx) * (x - cx) + (y - cy) * (y - cy);
      if (d2 > r * r) continue;
      const uint8_t v = uint8_t(k * (r * r + 1 - d2) / (r * r + 1));
      const uint8_t a[3] = {v, v, uint8_t(v + v / 6)};
      uint16_t& p = out[(y - y0) * w + x - x0];
      p = addLight(p, a, x, y);
    }
  }
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

void rainBounds(const Rain& r, int& x0, int& y0, int& x1, int& y1) {
  x1 = int(r.x) + 1;
  x0 = int(r.x - r.len * RAIN_SLOPE) - 1;
  y0 = int(r.y);
  y1 = int(r.y) + r.len + 1;
}

void compose(int x0, int y0, int w, int h, uint16_t* out) {
  // 1. tlo / zgaszona latarnia
  for (int r = 0; r < h; r++) {
    const int y = y0 + r;
    memcpy(out + r * w, BG + y * SCREEN_W + x0, w * 2);
    if (st.lampOff && y >= LAMP_Y && y < LAMP_Y + LAMP_H) {
      const int xa = x0 > LAMP_X ? x0 : LAMP_X;
      const int xb = (x0 + w) < (LAMP_X + LAMP_W) ? (x0 + w) : (LAMP_X + LAMP_W);
      if (xa < xb)
        memcpy(out + r * w + (xa - x0), LAMP_OFF + (y - LAMP_Y) * LAMP_W + (xa - LAMP_X), (xb - xa) * 2);
    }
  }

  // 2. okno naprzeciwko
  if (st.litOff)
    for (int y = LIT_Y0; y < LIT_Y1; y++)
      for (int x = LIT_X0; x < LIT_X1; x++)
        if (x >= x0 && x < x0 + w && y >= y0 && y < y0 + h) out[(y - y0) * w + x - x0] = LIT_DARK;

  // 3. postac
  if (st.figure >= 0) {
    const FigFrame& f = FIGURE[st.figure];
    const int ya = f.y > y0 ? f.y : y0, yb = (f.y + f.h) < (y0 + h) ? (f.y + f.h) : (y0 + h);
    const int xa = f.x > x0 ? f.x : x0, xb = (f.x + f.w) < (x0 + w) ? (f.x + f.w) : (x0 + w);
    for (int y = ya; y < yb; y++) {
      const uint8_t* src = f.data + (y - f.y) * f.w;
      for (int x = xa; x < xb; x++) {
        const uint8_t idx = src[x - f.x];
        if (!idx) continue;
        uint16_t& p = out[(y - y0) * w + x - x0];
        const uint8_t a = FIG_LUT_ALPHA[idx];
        p = a >= 250 ? FIG_LUT_COLOR[idx] : px::blend(p, FIG_LUT_COLOR[idx], a);
      }
    }
  }

  // 4. deszcz za szyba (tylko w obrebie ramy)
  for (int i = 0; i < RAIN_COUNT; i++) {
    const Rain& rn = st.rain[i];
    int bx0, by0, bx1, by1;
    rainBounds(rn, bx0, by0, bx1, by1);
    if (bx1 < x0 || bx0 >= x0 + w || by1 < y0 || by0 >= y0 + h) continue;
    for (int k = 0; k < rn.len; k++) {
      const int x = int(rn.x - k * RAIN_SLOPE), y = int(rn.y) + k;
      if (x < FRAME || x >= SCREEN_W - FRAME || y < FRAME || y >= SCREEN_H - FRAME) continue;
      if (x >= x0 && x < x0 + w && y >= y0 && y < y0 + h) {
        uint16_t& p = out[(y - y0) * w + x - x0];
        p = px::blend(p, RAIN_COLOR, RAIN_ALPHA);
      }
    }
  }

  // 5. krople na szybie: stale i splywajace
  for (int i = 0; i < DROP_COUNT; i++) {
    const Drop& d = DROPS[i];
    if (d.x + d.r < x0 || d.x - d.r >= x0 + w || d.y + d.r < y0 || d.y - d.r >= y0 + h) continue;
    drawBlob(d.x, d.y, d.r, d.k, x0, y0, w, h, out);
  }
  for (int i = 0; i < RUNNER_COUNT; i++) {
    const Runner& r = st.runners[i];
    drawBlob(int(r.x), int(r.y), r.r, 70, x0, y0, w, h, out);
  }

  // 6. odbicie budzika i daty
  if (y0 < DIGIT_Y + DIGIT_CELL_H && y0 + h > DIGIT_Y) {
    drawLight(COLON_DATA, COLON_W, DIGIT_CELL_H, COLON_X, DIGIT_Y, LUT_RED, x0, y0, w, h, out);
    for (int i = 0; i < 4; i++)
      if (st.digits[i] >= 0)
        drawLight(DIGIT_DATA[st.digits[i]], DIGIT_CELL_W, DIGIT_CELL_H, DIGIT_X[i], DIGIT_Y, LUT_RED,
                  x0, y0, w, h, out);
  }
  for (int i = 0; i < dateLen; i++) {
    if (dateChars[i].glyph < 0) continue;
    const DateGlyph& g = DATE_GLYPHS[dateChars[i].glyph];
    if (g.w)
      drawLight(DATE_DATA + g.offset, g.w, g.h, dateChars[i].x + g.xoff, DATE_BASELINE + g.yoff,
                LUT_DATE, x0, y0, w, h, out);
  }

  for (int i = 0; i < w * h; i++) out[i] = px::swap(out[i]);
}

}  // namespace scene
