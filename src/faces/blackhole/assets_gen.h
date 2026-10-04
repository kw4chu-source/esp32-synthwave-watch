#pragma once
// WYGENEROWANE przez tools/prerender/blackhole/gen_assets.py - nie edytowac recznie.
#include <stdint.h>

#include "faces/common_hd/hd.h"

namespace assets {

constexpr int DIGIT_CELL_W = 80, DIGIT_CELL_H = 91, DIGIT_TOP = 205;
constexpr int DIGIT_X[4] = {92, 156, 245, 309};
constexpr int COLON_X = 218, COLON_W = 45;
extern const uint8_t DIGIT_DATA[10][DIGIT_CELL_W * DIGIT_CELL_H];
extern const uint8_t COLON_DATA[COLON_W * DIGIT_CELL_H];

constexpr int DATE_BASELINE = 309, DATE_TOP = 284, DATE_BOTTOM = 316;
constexpr int DATE_GLYPH_COUNT = 31;
extern const hd::Glyph DATE_GLYPHS[DATE_GLYPH_COUNT];
extern const uint8_t DATE_DATA[];
extern const char* const WEEKDAYS[7];

constexpr int CX = 240, CY = 106;
constexpr int NR = 128;
constexpr uint8_t NONE = 255, SHADOW = 254;
constexpr int D_X = 12, D_Y = 66, D_W = 456, D_H = 81;
constexpr int L_X = 146, L_Y = 12, L_W = 188, L_H = 189;
// [promien | NONE | SHADOW, kat, jasnosc, poswiata]
extern const uint8_t D_MAP[D_W * D_H][4];
extern const uint8_t L_MAP[L_W * L_H][4];
extern const uint8_t TEX[NR][256];
extern const uint8_t RAMP[16][256][3];
extern const uint8_t HAZE[256][3];
extern const uint8_t LUT_CLOCK[256][3];
extern const uint8_t LUT_DATE[256][3];
struct Star { int16_t x, y; uint8_t b; };
constexpr int STAR_COUNT = 160;
extern const Star STARS[STAR_COUNT];

}  // namespace assets
