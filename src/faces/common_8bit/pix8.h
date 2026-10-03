#pragma once
// Silnik tarcz 8-bit: bufor 160x107 z paleta (styl NES), powiekszany x3.
// Rysowanie zmienia tylko bufor i oznacza kafelki 16x8 jako brudne;
// flush() wysyla brudne kafelki (laczone w poziome pasy) przez DMA.

#include <stdint.h>
#include <time.h>

namespace pix8 {

constexpr int LW = 160, LH = 107, SCALE = 3;
constexpr int TILE_W = 16, TILE_H = 8;
constexpr int TILES_X = LW / TILE_W, TILES_Y = (LH + TILE_H - 1) / TILE_H;

// Paleta (kolejnosc = wartosc w buforze)
enum : uint8_t { K, N, W, C, M, Y, G, R, O, B, P, S, D, L, T, PAL_COUNT };

extern uint8_t fb[LH][LW];

void begin(uint8_t background);
void markDirty(int x, int y, int w, int h);
void markAll();
void flush();

inline uint8_t get(int x, int y) { return (x >= 0 && x < LW && y >= 0 && y < LH) ? fb[y][x] : K; }
inline void set(int x, int y, uint8_t c) {
  if (x < 0 || x >= LW || y < 0 || y >= LH || fb[y][x] == c) return;
  fb[y][x] = c;
  markDirty(x, y, 1, 1);
}
void fill(int x, int y, int w, int h, uint8_t c);

// Sprite z wierszy znakow: '.' = przezroczysty, inny znak = piksel w kolorze
// `color` (albo kolor wg litery palety, gdy color == 0xFF: K N W C M Y G R O B P S D L T)
void sprite(int x, int y, const char* const* rows, int nrows, uint8_t color);
void spriteErase(int x, int y, const char* const* rows, int nrows, uint8_t bg);

// Tekst Press Start 2P (8 px, UTF-8 z polskimi wersalikami)
void text(int x, int y, const char* utf8, uint8_t color);
int textWidth(const char* utf8);
inline void textCentered(int y, const char* utf8, uint8_t color) { text((LW - textWidth(utf8)) / 2, y, utf8, color); }

// Duze cyfry 5x7 (godzina "HH:MM"), wycentrowane; dla kazdego zapalonego
// piksela czcionki wola cb(x, y, wiersz) z lewym gornym rogiem bloku scale x scale.
extern const uint8_t DIGITS5x7[10][7];
template <class Fn>
void bigTime(const char* hhmm, int y, int scale, Fn&& cb) {
  int total = 0;
  for (const char* p = hhmm; *p; p++) total += (*p == ':' ? 1 : 5) * scale + (p[1] ? scale : 0);
  int x = (LW - total) / 2;
  for (const char* p = hhmm; *p; p++) {
    if (*p == ':') {
      cb(x, y + 2 * scale, 2);
      cb(x, y + 4 * scale, 4);
      x += 2 * scale;
      continue;
    }
    const uint8_t* g = DIGITS5x7[*p - '0'];
    for (int r = 0; r < 7; r++)
      for (int k = 0; k < 5; k++)
        if (g[r] & (0x10 >> k)) cb(x + k * scale, y + r * scale, r);
    x += 6 * scale;
  }
}

// Pozycja i rozmiar obszaru godziny (do czyszczenia przy zmianie minuty)
inline int bigTimeWidth(int scale) { return (4 * 5 + 1 + 4) * scale; }

// "PIĄTEK 02.10.2026"
void formatDate(const struct tm* t, char* out, int len);

uint32_t rnd();
inline int rnd(int n) { return int(rnd() % uint32_t(n)); }

}  // namespace pix8
