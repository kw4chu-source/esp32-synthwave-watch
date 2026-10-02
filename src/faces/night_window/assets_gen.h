#pragma once
// WYGENEROWANE przez tools/prerender/night_window/gen_assets.py - nie edytowac recznie.
#include <stdint.h>

namespace assets {

constexpr int FRAME = 8;
constexpr int STREET_Y0 = 190, STREET_Y1 = 236;

extern const uint16_t BG[320 * 480];
constexpr int LAMP_X = 179, LAMP_Y = 62, LAMP_W = 253, LAMP_H = 187;
extern const uint16_t LAMP_OFF[LAMP_W * LAMP_H];

// zapalone okno naprzeciwko (gasnie/zapala sie)
constexpr int LIT_X0 = 186, LIT_Y0 = 146, LIT_X1 = 205, LIT_Y1 = 163;
constexpr uint16_t LIT_DARK = 8453;

// postac: klatka = minuta, piksel = (oczy4 << 4) | krycie4
struct FigFrame { int16_t x, y, w, h; const uint8_t* data; };
extern const FigFrame FIGURE[60];
extern const uint16_t FIG_LUT_COLOR[256];
extern const uint8_t FIG_LUT_ALPHA[256];

// odbicie budzika: index = (core4 << 4) | glow4 -> RGB888 dodawane do tla
constexpr int DIGIT_CELL_W = 89, DIGIT_CELL_H = 104, DIGIT_Y = 2;
constexpr int DIGIT_X[4] = {90, 155, 236, 301};
constexpr int COLON_X = 220, COLON_W = 40;
extern const uint8_t DIGIT_DATA[10][DIGIT_CELL_W * DIGIT_CELL_H];
extern const uint8_t COLON_DATA[COLON_W * DIGIT_CELL_H];
extern const uint8_t LUT_RED[256][3];

struct DateGlyph { uint16_t cp; uint8_t w, h; int8_t xoff, yoff; uint8_t adv; uint32_t offset; };
constexpr int DATE_BASELINE = 122, DATE_CENTER_X = 240;
constexpr int DATE_GLYPH_COUNT = 31;
extern const DateGlyph DATE_GLYPHS[DATE_GLYPH_COUNT];
extern const uint8_t DATE_DATA[];
extern const uint8_t LUT_DATE[256][3];
extern const char* const WEEKDAYS[7];

// krople na szybie
struct Drop { int16_t x, y; uint8_t r, k; };
constexpr int DROP_COUNT = 130;
extern const Drop DROPS[DROP_COUNT];

}  // namespace assets
