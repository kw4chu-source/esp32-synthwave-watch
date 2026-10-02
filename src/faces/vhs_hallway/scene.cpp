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
int dateWidth = 0;

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

inline uint32_t hash32(uint32_t v) {
  v ^= v >> 16;
  v *= 0x7FEB352D;
  v ^= v >> 15;
  v *= 0x846CA68B;
  return v ^ (v >> 16);
}

// Nakladanie sprite'a RGBA przycietego do prostokata
void drawSprite(int sx, int sy, int sw, int sh, const uint16_t* color, const uint8_t* alpha, int x0,
                int y0, int w, int h, uint16_t* out) {
  const int ya = sy > y0 ? sy : y0, yb = (sy + sh) < (y0 + h) ? (sy + sh) : (y0 + h);
  const int xa = sx > x0 ? sx : x0, xb = (sx + sw) < (x0 + w) ? (sx + sw) : (x0 + w);
  for (int y = ya; y < yb; y++) {
    const int row = (y - sy) * sw;
    uint16_t* dst = out + (y - y0) * w;
    for (int x = xa; x < xb; x++) {
      const uint8_t a = alpha[row + x - sx];
      if (!a) continue;
      uint16_t& p = dst[x - x0];
      p = a >= 250 ? color[row + x - sx] : px::blend(p, color[row + x - sx], a);
    }
  }
}

void drawFigure(int x0, int y0, int w, int h, uint16_t* out) {
  const FigFrame& f = FIGURE[st.figure];
  const int ya = f.y > y0 ? f.y : y0, yb = (f.y + f.h) < (y0 + h) ? (f.y + f.h) : (y0 + h);
  const int xa = f.x > x0 ? f.x : x0, xb = (f.x + f.w) < (x0 + w) ? (f.x + f.w) : (x0 + w);
  for (int y = ya; y < yb; y++) {
    const uint8_t* src = f.data + (y - f.y) * f.w;
    uint16_t* dst = out + (y - y0) * w;
    for (int x = xa; x < xb; x++) {
      const uint8_t idx = src[x - f.x];
      if (!idx) continue;
      const uint8_t a = FIG_LUT_ALPHA[idx];
      uint16_t& p = dst[x - x0];
      p = a >= 250 ? FIG_LUT_COLOR[idx] : px::blend(p, FIG_LUT_COLOR[idx], a);
    }
  }
}

// Pas zaklocen: przesuniety wiersz (tylko gdy prostokat obejmuje cala
// szerokosc) + jasny szum
void drawBand(int x0, int y0, int w, int h, uint16_t* out) {
  static uint16_t row[SCREEN_W];
  for (int y = st.bandY; y < st.bandY + BAND_H; y++) {
    if (y < y0 || y >= y0 + h) continue;
    uint16_t* line = out + (y - y0) * w;
    const uint32_t hv = hash32(st.noiseSeed * 977 + y);
    if (x0 == 0 && w == SCREEN_W) {
      const int shift = int(hv % 13) - 6;
      memcpy(row, line, sizeof(row));
      for (int x = 0; x < SCREEN_W; x++) line[x] = row[(x - shift + SCREEN_W) % SCREEN_W];
    }
    const uint8_t base = 30 + (hv >> 8) % 50;
    for (int i = 0; i < w; i++) {
      const uint32_t n = hash32(hv + (x0 + i) * 31);
      uint8_t a = base + (n & 31);
      if ((n >> 8) % 23 == 0) a = 200;  // biale iskry
      line[i] = px::blend(line[i], 0xFFFF, a);
    }
  }
}

}  // namespace

void setDate(const char* utf8) {
  int n = 0;
  int x = DATE_X;
  for (const char* p = utf8; *p && n < MAX_DATE_CHARS;) {
    const int g = findDateGlyph(decodeUtf8(p));
    dateChars[n++] = {int16_t(x), int8_t(g)};
    if (g >= 0) x += DATE_GLYPHS[g].adv;
  }
  dateLen = n;
  dateWidth = x - DATE_X;
}

void dateRect(int& x, int& y, int& w, int& h) {
  x = DATE_X - 4;
  y = DATE_Y - 2;
  w = 300;  // najdluzsza data ("PONIEDZIAŁEK 28.09.2026") + cien
  h = 34;
}

void compose(int x0, int y0, int w, int h, uint16_t* out) {
  if (st.black) {
    memset(out, 0, w * h * 2);
    return;
  }

  // 1. tlo, z fragmentem zgaszonej swietlowki
  for (int r = 0; r < h; r++) {
    const int y = y0 + r;
    memcpy(out + r * w, BG + y * SCREEN_W + x0, w * 2);
    if (st.lampOff && y >= LAMP_Y && y < LAMP_Y + LAMP_H) {
      const int xa = x0 > LAMP_X ? x0 : LAMP_X;
      const int xb = (x0 + w) < (LAMP_X + LAMP_W) ? (x0 + w) : (LAMP_X + LAMP_W);
      if (xa < xb)
        memcpy(out + r * w + (xa - x0), LAMP_OFF + (y - LAMP_Y) * LAMP_W + (xa - LAMP_X),
               (xb - xa) * 2);
    }
  }

  // 2. postac, 3. nakladki kamery
  drawFigure(x0, y0, w, h, out);
  for (int i = 0; i < 4; i++)
    if (st.digits[i] >= 0)
      drawSprite(DIGIT_X[i], DIGIT_Y, DIGIT_CELL_W, DIGIT_CELL_H, DIGIT_COLOR[st.digits[i]],
                 DIGIT_ALPHA[st.digits[i]], x0, y0, w, h, out);
  drawSprite(COLON.x, COLON.y, COLON.w, COLON.h, COLON.color, COLON.alpha, x0, y0, w, h, out);
  if (st.recOn)
    drawSprite(REC_DOT.x, REC_DOT.y, REC_DOT.w, REC_DOT.h, REC_DOT.color, REC_DOT.alpha, x0, y0,
               w, h, out);
  for (int i = 0; i < dateLen; i++) {
    if (dateChars[i].glyph < 0) continue;
    const DateGlyph& g = DATE_GLYPHS[dateChars[i].glyph];
    if (g.w)
      drawSprite(dateChars[i].x + g.xoff, DATE_Y + g.yoff, g.w, g.h, g.color, g.alpha, x0, y0, w,
                 h, out);
  }

  // 4. pas zaklocen tasmy
  if (st.bandY > -BAND_H && st.bandY < SCREEN_H) drawBand(x0, y0, w, h, out);

  for (int i = 0; i < w * h; i++) out[i] = px::swap(out[i]);
}

}  // namespace scene
