#pragma once
// WYGENEROWANE przez tools/prerender/portal/gen_assets.py - nie edytowac recznie.
#include <stdint.h>

#include "faces/common_hd/hd.h"

namespace assets {

constexpr int DIGIT_CELL_W = 71, DIGIT_CELL_H = 106, DIGIT_TOP = 87;
constexpr int DIGIT_X[4] = {103, 164, 246, 307};
constexpr int COLON_X = 223, COLON_W = 35;
extern const uint8_t DIGIT_DATA[10][DIGIT_CELL_W * DIGIT_CELL_H];
extern const uint8_t COLON_DATA[COLON_W * DIGIT_CELL_H];

constexpr int DATE_BASELINE = 298, DATE_TOP = 271, DATE_BOTTOM = 310;
constexpr int DATE_GLYPH_COUNT = 31;
extern const hd::Glyph DATE_GLYPHS[DATE_GLYPH_COUNT];
extern const uint8_t DATE_DATA[];
extern const char* const WEEKDAYS[7];

constexpr int CX = 240, CY = 140, RX = 150, RY = 112, R_ONE = 200;
constexpr int MAP_X = 73, MAP_Y = 14, MAP_W = 335, MAP_H = 252;
// [r8, kat16] na piksel; r8 = R_ONE na krawedzi elipsy
extern const uint16_t PORTAL_MAP[MAP_W * MAP_H][2];
struct Star { int16_t x, y; uint8_t b; };
constexpr int STAR_COUNT = 80;
extern const Star STARS[STAR_COUNT];

}  // namespace assets
