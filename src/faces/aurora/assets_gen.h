#pragma once
// WYGENEROWANE przez tools/prerender/aurora/gen_assets.py - nie edytowac recznie.
#include <stdint.h>

#include "faces/common_hd/hd.h"

namespace assets {

constexpr int DIGIT_CELL_W = 88, DIGIT_CELL_H = 100, DIGIT_TOP = 14;
constexpr int DIGIT_X[4] = {72, 144, 248, 320};
constexpr int COLON_X = 214, COLON_W = 52;
extern const uint8_t DIGIT_DATA[10][DIGIT_CELL_W * DIGIT_CELL_H];
extern const uint8_t COLON_DATA[COLON_W * DIGIT_CELL_H];

constexpr int DATE_BASELINE = 309, DATE_TOP = 284, DATE_BOTTOM = 316;
constexpr int DATE_GLYPH_COUNT = 31;
extern const hd::Glyph DATE_GLYPHS[DATE_GLYPH_COUNT];
extern const uint8_t DATE_DATA[];
extern const char* const WEEKDAYS[7];

constexpr int HORIZON = 222;
extern const int16_t RIDGE[480];
extern const int16_t RIDGE2[480];
extern const uint8_t RAYS[480];
struct Star { int16_t x, y; uint8_t b, phase; };
constexpr int STAR_COUNT = 150;
extern const Star STARS[STAR_COUNT];
extern const uint8_t LUT_CLOCK[256][3];
extern const uint8_t LUT_DATE[256][3];

}  // namespace assets
