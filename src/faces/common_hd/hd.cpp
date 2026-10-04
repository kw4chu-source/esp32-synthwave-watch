#include "hd.h"

namespace hd {

void drawGlyph(const uint8_t* data, int gw, int gh, int gx, int gy, const Style& st, int x0, int y0, int w, int h,
               uint16_t* out) {
  const int ya = gy > y0 ? gy : y0, yb = (gy + gh) < (y0 + h) ? (gy + gh) : (y0 + h);
  const int xa = gx > x0 ? gx : x0, xb = (gx + gw) < (x0 + w) ? (gx + gw) : (x0 + w);
  for (int y = ya; y < yb; y++) {
    const uint8_t* src = data + (y - gy) * gw;
    uint16_t* dst = out + (y - y0) * w;
    for (int x = xa; x < xb; x++) {
      const uint8_t v = src[x - gx];
      if (!v) continue;
      uint16_t& p = dst[x - x0];
      switch (st.ink) {
        case Ink::Neon:
          p = addLight(p, st.lut[v], x, y);
          break;
        case Ink::NeonShadow:
          p = px::blend32(p, 0, (v & 15) * 3 / 2);  // do ~70% przyciemnienia pod poswiata
          p = addLight(p, st.lut[v], x, y);
          break;
        case Ink::Outline:
          if (v & 15) p = px::blend32(p, st.stroke, ((v & 15) * 32 + 7) / 15);
          if (v >> 4) p = px::blend32(p, st.fill, ((v >> 4) * 32 + 7) / 15);
          break;
      }
    }
  }
}

namespace {
int decodeUtf8(const char*& s) {
  const uint8_t c = *s++;
  if (c < 0x80) return c;
  if ((c & 0xE0) == 0xC0) return ((c & 0x1F) << 6) | (*s++ & 0x3F);
  if ((c & 0xF0) == 0xE0) {
    const int v = ((c & 0x0F) << 12) | ((s[0] & 0x3F) << 6) | (s[1] & 0x3F);
    s += 2;
    return v;
  }
  return '?';
}
}  // namespace

void TextLine::set(const char* utf8, const Glyph* glyphs, int count, int cx) {
  _glyphs = glyphs;
  _len = 0;
  int width = 0;
  for (const char* p = utf8; *p && _len < 40;) {
    const int cp = decodeUtf8(p);
    int8_t gi = -1;
    for (int g = 0; g < count; g++)
      if (glyphs[g].cp == cp) gi = g;
    _ch[_len++].glyph = gi;
    if (gi >= 0) width += glyphs[gi].adv;
  }
  int x = cx - width / 2;
  for (int i = 0; i < _len; i++) {
    _ch[i].x = x;
    if (_ch[i].glyph >= 0) x += _glyphs[_ch[i].glyph].adv;
  }
}

void TextLine::draw(const uint8_t* data, int baseline, const Style& st, int x0, int y0, int w, int h,
                    uint16_t* out) const {
  for (int i = 0; i < _len; i++) {
    if (_ch[i].glyph < 0) continue;
    const Glyph& g = _glyphs[_ch[i].glyph];
    if (g.w) drawGlyph(data + g.offset, g.w, g.h, _ch[i].x + g.xoff, baseline + g.yoff, st, x0, y0, w, h, out);
  }
}

void Tiles::mark(int x, int y, int w, int h) {
  if (w <= 0 || h <= 0 || x + w <= 0 || y + h <= 0 || x >= SCREEN_W || y >= SCREEN_H) return;
  int tx0 = x < 0 ? 0 : x / TW, ty0 = y < 0 ? 0 : y / TH;
  int tx1 = (x + w - 1) / TW, ty1 = (y + h - 1) / TH;
  if (tx1 >= NX) tx1 = NX - 1;
  if (ty1 >= NY) ty1 = NY - 1;
  const uint32_t mask = ((tx1 - tx0 + 1) >= 32 ? 0xFFFFFFFFu : ((1u << (tx1 - tx0 + 1)) - 1)) << tx0;
  for (int ty = ty0; ty <= ty1; ty++) _rows[ty] |= mask;
}

void Tiles::markAll() {
  for (auto& r : _rows) r = (1u << NX) - 1;
}

}  // namespace hd
