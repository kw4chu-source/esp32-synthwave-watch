#pragma once
// WYGENEROWANE przez tools/prerender/gen_assets.py - nie edytowac recznie.
#include <stdint.h>

namespace assets {

// ---- uklad ----
constexpr int DIGIT_FONT_PX = 117;
constexpr int DIGIT_CELL_W = 110;
constexpr int DIGIT_CELL_H = 97;
constexpr int DIGIT_TOP = 50;
constexpr int DIGIT_X[4] = {8, 112, 258, 362};
constexpr int COLON_X = 214;
constexpr int COLON_W = 40;
constexpr int GLOW_PAD = 6;

// ---- cyfry: index piksela = (core4 << 4) | glow4 -> LUT ----
extern const uint16_t GLYPH_LUT_COLOR[256];
extern const uint8_t GLYPH_LUT_ALPHA[256];
extern const uint8_t DIGIT_DATA[10][DIGIT_CELL_W * DIGIT_CELL_H];
extern const uint8_t COLON_DATA[COLON_W * DIGIT_CELL_H];

// ---- data (Audiowide 20px, alpha 8-bit) ----
struct DateGlyph { uint16_t cp; uint8_t w, h; int8_t xoff, yoff; uint8_t adv; uint32_t offset; };
constexpr int DATE_GLYPH_COUNT = 31;
constexpr int DATE_BASELINE = 29;
constexpr uint8_t DATE_R = 200, DATE_G = 130, DATE_B = 255;
extern const DateGlyph DATE_GLYPHS[DATE_GLYPH_COUNT];
extern const uint8_t DATE_ALPHA[];
extern const char* const WEEKDAYS[7];

// ---- tlo: RGB888 na wiersz (dithering na urzadzeniu) ----
extern const uint8_t SKY_RGB[220][3];
extern const uint8_t GROUND_RGB[100][3];

// ---- slonce ----
constexpr int SUN_CX = 240, SUN_CY = 220, SUN_R = 108;
constexpr int SUN_FRAMES = 12;
constexpr int SUN_SLICE_ZONE_TOP = 37;  // wiersz slonca, od ktorego sa przeciecia
extern const uint8_t SUN_HALFW[SUN_R];
extern const uint8_t SUN_RGB[SUN_R][3];
extern const uint8_t SUN_GAP[SUN_FRAMES][SUN_R];  // 1 = przeciecie (tlo nieba)

// ---- gwiazdy ----
struct Star { uint16_t x; uint8_t y, phase, size, bright; };
constexpr int STAR_COUNT = 46;
extern const Star STARS[STAR_COUNT];

}  // namespace assets
