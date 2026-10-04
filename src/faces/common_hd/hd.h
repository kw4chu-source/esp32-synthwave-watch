#pragma once
// Wspolne narzedzia tarcz pelnej rozdzielczosci: swiatlo dodawane, glify
// (neon / obrys), linia daty, kafelki brudnych prostokatow 16x8.
// Glify generuje tools/prerender/common_hd.py: 1 B/piksel = (hi4 << 4) | lo4.

#include <stdint.h>

#include "config.h"
#include "core/pixel.h"

namespace hd {

struct Glyph {
  uint16_t cp;
  uint8_t w, h;
  int8_t xoff, yoff;
  uint8_t adv;
  uint32_t offset;
};

// Swiatlo RGB888 dodawane do piksela 565 z nasyceniem i ditheringiem Bayera
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

// Jak wyzej, ale skladowe int (moga byc > 255)
inline uint16_t addRgb(uint16_t p, int r8, int g8, int b8, int x, int y) {
  const uint8_t t = px::BAYER4[y & 3][x & 3];
  int r = (p >> 11) + ((r8 + (t >> 1)) >> 3);
  int g = ((p >> 5) & 63) + ((g8 + (t >> 2)) >> 2);
  int b = (p & 31) + ((b8 + (t >> 1)) >> 3);
  if (r > 31) r = 31;
  if (g > 63) g = 63;
  if (b > 31) b = 31;
  return uint16_t((r << 11) | (g << 5) | b);
}

enum class Ink : uint8_t {
  Neon,        // hi = rdzen, lo = poswiata -> lut[idx] dodawane
  NeonShadow,  // jak Neon, ale najpierw tlo przyciemnione proporcjonalnie do poswiaty
  Outline,     // lo = obrys (kolor stroke), hi = wypelnienie (kolor fill)
};

struct Style {
  Ink ink;
  const uint8_t (*lut)[3];
  uint16_t fill, stroke;  // RGB565 (Outline)
};

// Glif przyciety do kawalka (x0, y0, w, h); out w zwyklym RGB565 (przed swap)
void drawGlyph(const uint8_t* data, int gw, int gh, int gx, int gy, const Style& st, int x0, int y0, int w, int h,
               uint16_t* out);

// Linia tekstu (data) wysrodkowana na cx, glify z atlasu
class TextLine {
public:
  void set(const char* utf8, const Glyph* glyphs, int count, int cx);
  void draw(const uint8_t* data, int baseline, const Style& st, int x0, int y0, int w, int h, uint16_t* out) const;

private:
  struct Ch {
    int16_t x;
    int8_t glyph;
  };
  Ch _ch[40];
  int _len = 0;
  const Glyph* _glyphs = nullptr;
};

// Kafelki 16x8 calego ekranu: mark() zbiera, flush() wysyla polaczone poziomo
class Tiles {
public:
  static constexpr int TW = 16, TH = 8, NX = SCREEN_W / TW, NY = SCREEN_H / TH;
  void mark(int x, int y, int w, int h);
  void markAll();
  template <class Compose>
  void flush(Compose&& compose);

private:
  uint32_t _rows[NY] = {};
};

}  // namespace hd

#include "core/push.h"

template <class Compose>
void hd::Tiles::flush(Compose&& compose) {
  for (int ty = 0; ty < NY; ty++) {
    uint32_t bits = _rows[ty];
    if (!bits) continue;
    // sasiednie wiersze z ta sama maska wysylane jednym prostokatem
    int ty2 = ty;
    while (ty2 + 1 < NY && _rows[ty2 + 1] == bits) ty2++;
    for (int k = ty; k <= ty2; k++) _rows[k] = 0;
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
      pushComposed(tx * TW, ty * TH, run * TW, (ty2 - ty + 1) * TH, compose);
      tx += run;
    }
    ty = ty2;
  }
}
