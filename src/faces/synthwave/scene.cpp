#include "scene.h"

#include <string.h>
#include "core/pixel.h"

using namespace assets;

Scene scene;

namespace {

// Amplituda poziomego "rozjechania" wierszy w kolejnych krokach przejscia cyfry
constexpr uint8_t JITTER_AMP[Scene::TRANSITION_STEPS + 1] = {0, 5, 12, 18, 18, 10, 4};

// Sinus 0..255, 32 probki - migotanie gwiazd
constexpr uint8_t TWINKLE[32] = {128, 152, 176, 198, 218, 234, 245, 253, 255, 253, 245,
                                 234, 218, 198, 176, 152, 128, 103, 79,  57,  37,  21,
                                 10,  2,   0,   2,   10,  21,  37,  57,  79,  103};

inline uint32_t hash32(uint32_t v) {
  v ^= v >> 16;
  v *= 0x7FEB352D;
  v ^= v >> 15;
  v *= 0x846CA68B;
  v ^= v >> 16;
  return v;
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

}  // namespace

void Scene::begin() {
  for (auto& s : _slots) s = Slot{};
  _dateLen = 0;
}

void Scene::setDigit(int slot, int8_t value, bool animate) {
  Slot& s = _slots[slot];
  if (s.to == value && s.step == 0) return;
  s.from = animate ? s.to : value;
  s.to = value;
  s.step = animate ? 1 : 0;
}

void Scene::advanceTransition(int slot) {
  Slot& s = _slots[slot];
  if (s.step == 0) return;
  if (++s.step > TRANSITION_STEPS) s.step = 0;
}

void Scene::setDate(const char* utf8) {
  int cps[MAX_DATE_CHARS];
  int n = 0, width = 0;
  for (const char* p = utf8; *p && n < MAX_DATE_CHARS;) {
    cps[n] = decodeUtf8(p);
    int g = findDateGlyph(cps[n]);
    width += g >= 0 ? DATE_GLYPHS[g].adv : 0;
    n++;
  }
  int x = (SCREEN_W - width) / 2;
  for (int i = 0; i < n; i++) {
    int g = findDateGlyph(cps[i]);
    _date[i] = {int16_t(x), int8_t(g)};
    if (g >= 0) x += DATE_GLYPHS[g].adv;
  }
  _dateLen = n;
}

uint8_t Scene::starLevel(int i) const {
  const Star& s = STARS[i];
  const uint32_t speed = 1 + (s.phase & 1);
  const uint8_t wave = TWINKLE[((s.phase >> 3) + ((starTick * speed) >> 1)) & 31];
  // 30%..100% jasnosci bazowej, kwantyzacja do 16 poziomow = rzadkie odswiezenia
  uint32_t lvl = s.bright * (77 + (178 * wave >> 8)) >> 8;
  return uint8_t(lvl & 0xF0);
}

void Scene::drawGlyph(const uint8_t* data, int gw, int gx, int gy, int jitterSeed,
                      uint8_t step, int x0, int y0, int w, int h, uint16_t* out) const {
  const int gh = DIGIT_CELL_H;
  const int ya = gy > y0 ? gy : y0;
  const int yb = (gy + gh) < (y0 + h) ? (gy + gh) : (y0 + h);
  const int xa = gx > x0 ? gx : x0;
  const int xb = (gx + gw) < (x0 + w) ? (gx + gw) : (x0 + w);
  if (ya >= yb || xa >= xb) return;

  const uint8_t amp = JITTER_AMP[step];
  for (int y = ya; y < yb; y++) {
    const int sy = y - gy;
    int shift = 0;
    if (amp) {
      uint32_t hv = hash32(uint32_t((sy >> 2) * 31 + step * 977 + jitterSeed * 7919));
      if ((hv & 3) == 0) shift = int((hv >> 8) % (2 * amp + 1)) - amp;
    }
    const uint8_t* src = data + sy * gw;
    uint16_t* dst = out + (y - y0) * w;
    for (int x = xa; x < xb; x++) {
      const int sx = x - gx - shift;
      if (sx < 0 || sx >= gw) continue;
      const uint8_t idx = src[sx];
      if (!idx) continue;
      const uint8_t a = GLYPH_LUT_ALPHA[idx];
      uint16_t& p = dst[x - x0];
      p = a >= 248 ? GLYPH_LUT_COLOR[idx] : px::blend(p, GLYPH_LUT_COLOR[idx], a);
    }
  }
}

void Scene::drawDate(int x0, int y0, int w, int h, uint16_t* out) const {
  const uint16_t color = px::rgb565(DATE_R, DATE_G, DATE_B);
  for (int i = 0; i < _dateLen; i++) {
    if (_date[i].glyph < 0) continue;
    const DateGlyph& g = DATE_GLYPHS[_date[i].glyph];
    if (!g.w) continue;
    const int gx = _date[i].x + g.xoff;
    const int gy = DATE_BASELINE + g.yoff;
    const int ya = gy > y0 ? gy : y0;
    const int yb = (gy + g.h) < (y0 + h) ? (gy + g.h) : (y0 + h);
    const int xa = gx > x0 ? gx : x0;
    const int xb = (gx + g.w) < (x0 + w) ? (gx + g.w) : (x0 + w);
    for (int y = ya; y < yb; y++) {
      const uint8_t* src = DATE_ALPHA + g.offset + (y - gy) * g.w;
      uint16_t* dst = out + (y - y0) * w;
      for (int x = xa; x < xb; x++) {
        const uint8_t a = src[x - gx];
        if (a) dst[x - x0] = px::blend(dst[x - x0], color, a);
      }
    }
  }
}

void Scene::compose(int x0, int y0, int w, int h, uint16_t* out) const {
  const int sunTop = SUN_CY - SUN_R;

  for (int r = 0; r < h; r++) {
    const int y = y0 + r;
    uint16_t* line = out + r * w;

    uint16_t bg[4];
    px::ditherRow(SKY_RGB[y], y, bg);
    for (int i = 0; i < w; i++) line[i] = bg[(x0 + i) & 3];

    if (y >= sunTop && y < SUN_CY) {
      const int si = y - sunTop;
      if (!SUN_GAP[sunFrame][si]) {
        uint16_t sc[4];
        px::ditherRow(SUN_RGB[si], y, sc);
        int xa = SUN_CX - SUN_HALFW[si], xb = SUN_CX + SUN_HALFW[si];
        if (xa < x0) xa = x0;
        if (xb > x0 + w) xb = x0 + w;
        for (int x = xa; x < xb; x++) line[x - x0] = sc[x & 3];
      }
    }
  }

  for (int i = 0; i < STAR_COUNT; i++) {
    const Star& s = STARS[i];
    if (s.x + s.size <= x0 || s.x >= x0 + w || s.y + s.size <= y0 || s.y >= y0 + h) continue;
    const uint8_t l = starLevel(i);
    const uint16_t c = px::rgb565(l, l, l + 30 > 255 ? 255 : l + 30);
    for (int dy = 0; dy < s.size; dy++)
      for (int dx = 0; dx < s.size; dx++) {
        const int x = s.x + dx, y = s.y + dy;
        if (x >= x0 && x < x0 + w && y >= y0 && y < y0 + h) out[(y - y0) * w + (x - x0)] = c;
      }
  }

  if (y0 < DATE_BOTTOM) drawDate(x0, y0, w, h, out);

  if (y0 < DIGIT_TOP + DIGIT_CELL_H && y0 + h > DIGIT_TOP) {
    drawGlyph(COLON_DATA, COLON_W, COLON_X, DIGIT_TOP, 0, 0, x0, y0, w, h, out);
    for (int i = 0; i < DIGIT_SLOTS; i++) {
      const Slot& s = _slots[i];
      const int8_t v = (s.step != 0 && s.step <= TRANSITION_STEPS / 2) ? s.from : s.to;
      if (v < 0) continue;
      drawGlyph(DIGIT_DATA[v], DIGIT_CELL_W, DIGIT_X[i], DIGIT_TOP, i + 1, s.step, x0, y0, w, h,
                out);
    }
  }

  for (int i = 0; i < w * h; i++) out[i] = px::swap(out[i]);
}
