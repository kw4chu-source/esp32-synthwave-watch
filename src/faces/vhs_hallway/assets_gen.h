#pragma once
// WYGENEROWANE przez tools/prerender/vhs_hallway/gen_assets.py - nie edytowac recznie.
#include <stdint.h>

namespace assets {

// Sprite RGBA: kolor RGB565 + alfa 0-255, pozycja na ekranie
struct Sprite { int16_t x, y, w, h; const uint16_t* color; const uint8_t* alpha; };

extern const uint16_t BG[320 * 480];

// fragment tla przy zgaszonej swietlowce
constexpr int LAMP_X = 88, LAMP_Y = 33, LAMP_W = 305, LAMP_H = 197;
extern const uint16_t LAMP_OFF[LAMP_W * LAMP_H];

// cyfry: wspolna komorka, x zalezy od slotu
constexpr int DIGIT_CELL_W = 51, DIGIT_CELL_H = 78, DIGIT_Y = 14;
constexpr int DIGIT_X[4] = {121, 168, 262, 309};
extern const uint16_t DIGIT_COLOR[10][DIGIT_CELL_W * DIGIT_CELL_H];
extern const uint8_t DIGIT_ALPHA[10][DIGIT_CELL_W * DIGIT_CELL_H];
extern const Sprite COLON;

// data (VT323 26px)
struct DateGlyph { uint16_t cp; uint8_t w, h; int8_t xoff, yoff; uint8_t adv; const uint16_t* color; const uint8_t* alpha; };
constexpr int DATE_X = 16, DATE_Y = 286;
constexpr int DATE_GLYPH_COUNT = 31;
extern const DateGlyph DATE_GLYPHS[DATE_GLYPH_COUNT];
extern const char* const WEEKDAYS[7];

extern const Sprite REC_DOT;

// postac: klatka na kazda minute (0 = koniec korytarza, 59 = przed kamera)
// piksel = (oczy4 << 4) | krycie4 -> FIG_LUT_COLOR / FIG_LUT_ALPHA
struct FigFrame { int16_t x, y, w, h; const uint8_t* data; };
extern const FigFrame FIGURE[60];
extern const uint16_t FIG_LUT_COLOR[256];
extern const uint8_t FIG_LUT_ALPHA[256];

}  // namespace assets
