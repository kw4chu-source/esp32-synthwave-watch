#include "pix8.h"

#include <esp_random.h>
#include <stdio.h>
#include <string.h>

#include "config.h"
#include "core/pixel.h"
#include "core/push.h"
#include "pixel_font_gen.h"

namespace pix8 {

uint8_t fb[LH][LW];

const uint8_t DIGITS5x7[10][7] = {
    {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E}, {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E},
    {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F}, {0x1E, 0x01, 0x01, 0x0E, 0x01, 0x01, 0x1E},
    {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02}, {0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E},
    {0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E}, {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08},
    {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E}, {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C},
};

namespace {

// Paleta NES-owa (RGB565, juz w kolejnosci bajtow DMA)
uint16_t palSw[PAL_COUNT];
const uint8_t PAL_RGB[PAL_COUNT][3] = {
    {0, 0, 0},       {12, 10, 34},   {252, 252, 252}, {0, 232, 216},  {248, 56, 152},
    {248, 216, 0},   {0, 200, 60},   {232, 40, 24},   {248, 120, 0},  {40, 80, 248},
    {152, 72, 248},  {110, 110, 140}, {40, 36, 70},   {130, 200, 255}, {0, 120, 40},
};

uint16_t dirty[TILES_Y];  // bit = kafelek w wierszu

int paletteIndex(char ch) {
  static const char* const LETTERS = "KNWCMYGROBPSDLT";
  const char* p = strchr(LETTERS, ch);
  return p ? int(p - LETTERS) : W;
}

int glyphIndex(int cp) {
  for (int i = 0; i < FONT_COUNT; i++)
    if (FONT_CP[i] == cp) return i;
  return 0;  // spacja
}

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

void compose(int x0, int y0, int w, int h, uint16_t* out) {
  for (int r = 0; r < h; r++) {
    const int ly = (y0 + r) / SCALE;
    const uint8_t* src = fb[ly < LH ? ly : LH - 1];
    uint16_t* dst = out + r * w;
    for (int i = 0; i < w; i++) dst[i] = palSw[src[(x0 + i) / SCALE]];
  }
}

}  // namespace

void begin(uint8_t background) {
  for (int i = 0; i < PAL_COUNT; i++)
    palSw[i] = px::swap(px::rgb565(PAL_RGB[i][0], PAL_RGB[i][1], PAL_RGB[i][2]));
  memset(fb, background, sizeof(fb));
  markAll();
}

void markDirty(int x, int y, int w, int h) {
  if (w <= 0 || h <= 0) return;
  int tx0 = x / TILE_W, tx1 = (x + w - 1) / TILE_W, ty0 = y / TILE_H, ty1 = (y + h - 1) / TILE_H;
  if (tx0 < 0) tx0 = 0;
  if (ty0 < 0) ty0 = 0;
  if (tx1 >= TILES_X) tx1 = TILES_X - 1;
  if (ty1 >= TILES_Y) ty1 = TILES_Y - 1;
  for (int ty = ty0; ty <= ty1; ty++)
    for (int tx = tx0; tx <= tx1; tx++) dirty[ty] |= 1u << tx;
}

void markAll() {
  for (auto& d : dirty) d = (1u << TILES_X) - 1;
}

void flush() {
  for (int ty = 0; ty < TILES_Y; ty++) {
    uint16_t bits = dirty[ty];
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
      pushComposed(tx * TILE_W * SCALE, ty * TILE_H * SCALE, run * TILE_W * SCALE, TILE_H * SCALE, compose);
      tx += run;
    }
  }
}

void fill(int x, int y, int w, int h, uint8_t c) {
  for (int yy = y; yy < y + h; yy++)
    for (int xx = x; xx < x + w; xx++)
      if (xx >= 0 && xx < LW && yy >= 0 && yy < LH) fb[yy][xx] = c;
  markDirty(x, y, w, h);
}

void sprite(int x, int y, const char* const* rows, int nrows, uint8_t color) {
  for (int j = 0; j < nrows; j++)
    for (int i = 0; rows[j][i]; i++)
      if (rows[j][i] != '.') set(x + i, y + j, color == 0xFF ? paletteIndex(rows[j][i]) : color);
}

void spriteErase(int x, int y, const char* const* rows, int nrows, uint8_t bg) {
  for (int j = 0; j < nrows; j++)
    for (int i = 0; rows[j][i]; i++)
      if (rows[j][i] != '.') set(x + i, y + j, bg);
}

void text(int x, int y, const char* utf8, uint8_t color) {
  for (const char* p = utf8; *p;) {
    const uint8_t* g = FONT_ROWS[glyphIndex(decodeUtf8(p))];
    for (int r = 0; r < FONT_CELL_H; r++)
      for (int k = 0; k < 8; k++)
        if (g[r] & (0x80 >> k)) set(x + k, y - FONT_TOP + r, color);
    x += 8;
  }
}

int textWidth(const char* utf8) {
  int n = 0;
  for (const char* p = utf8; *p;) {
    decodeUtf8(p);
    n++;
  }
  return n * 8;
}

uint32_t rnd() { return esp_random(); }

void formatDate(const struct tm* t, char* out, int len) {
  static const char* const DAYS[7] = {"NIEDZIELA", "PONIEDZIAŁEK", "WTOREK", "ŚRODA",
                                      "CZWARTEK", "PIĄTEK", "SOBOTA"};
  snprintf(out, len, "%s %02d.%02d.%04d", DAYS[t->tm_wday], t->tm_mday, t->tm_mon + 1,
           t->tm_year + 1900);
}

}  // namespace pix8
