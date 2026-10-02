#pragma once
// Operacje na pikselach RGB565 + dithering Bayer 4x4 (ten sam co w gen_assets.py).

#include <stdint.h>

namespace px {

constexpr uint8_t BAYER4[4][4] = {
    {0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};

inline uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

// RGB888 -> RGB565 z progiem Bayera dla pozycji (x, y)
inline uint16_t dither(uint8_t r, uint8_t g, uint8_t b, int x, int y) {
  const uint8_t t = BAYER4[y & 3][x & 3];
  int r5 = (r + (t >> 1)) >> 3;  // t*8/16
  int g6 = (g + (t >> 2)) >> 2;  // t*4/16
  int b5 = (b + (t >> 1)) >> 3;
  if (r5 > 31) r5 = 31;
  if (g6 > 63) g6 = 63;
  if (b5 > 31) b5 = 31;
  return (r5 << 11) | (g6 << 5) | b5;
}

// 4 warianty koloru wiersza (dla x & 3) - dithering bez liczenia per piksel
inline void ditherRow(const uint8_t rgb[3], int y, uint16_t out[4]) {
  for (int i = 0; i < 4; i++) out[i] = dither(rgb[0], rgb[1], rgb[2], i, y);
}

// Mieszanie 565, alpha 0-32 (rownolegle pola R/G/B w jednym slowie 32-bit)
inline uint16_t blend32(uint16_t bg, uint16_t fg, uint32_t a32) {
  uint32_t b = (bg | (uint32_t(bg) << 16)) & 0x07E0F81F;
  uint32_t f = (fg | (uint32_t(fg) << 16)) & 0x07E0F81F;
  uint32_t r = ((((f - b) * a32) >> 5) + b) & 0x07E0F81F;
  return uint16_t((r >> 16) | r);
}

inline uint16_t blend(uint16_t bg, uint16_t fg, uint8_t a) {
  return blend32(bg, fg, (uint32_t(a) * 32 + 128) >> 8);
}

// Bufory DMA LovyanGFX (swap565_t) trzymaja piksel big-endian
inline uint16_t swap(uint16_t c) { return uint16_t((c >> 8) | (c << 8)); }

}  // namespace px
