#pragma once
// WYGENEROWANE przez tools/prerender/tunnel/gen_assets.py - nie edytowac recznie.
#include <stdint.h>

namespace assets {

constexpr int DIGIT_CELL_W = 80, DIGIT_CELL_H = 91, DIGIT_TOP = 105;
constexpr int DIGIT_X[4] = {91, 155, 244, 308};
constexpr int COLON_X = 217, COLON_W = 45;
extern const uint8_t DIGIT_DATA[10][DIGIT_CELL_W * DIGIT_CELL_H];
extern const uint8_t COLON_DATA[COLON_W * DIGIT_CELL_H];

// tablica pod cyframi: maska wypelnienia + zolta ramka (neon)
constexpr int PLATE_X = 102, PLATE_Y = 92, PLATE_W = 276, PLATE_H = 116;
extern const uint8_t PLATE_FILL[PLATE_W * PLATE_H];
extern const uint8_t PLATE_GLOW[PLATE_W * PLATE_H];

struct DateGlyph { uint16_t cp; uint8_t w, h; int8_t xoff, yoff; uint8_t adv; uint32_t offset; };
constexpr int DATE_BASELINE = 227, DATE_CENTER_X = 240;
constexpr int DATE_GLYPH_COUNT = 31;
extern const DateGlyph DATE_GLYPHS[DATE_GLYPH_COUNT];
extern const uint8_t DATE_DATA[];
extern const char* const WEEKDAYS[7];

extern const uint8_t LUT_CYAN[256][3];
extern const uint8_t LUT_YELLOW[256][3];
extern const uint8_t LUT_MAGENTA[256][3];

}  // namespace assets
