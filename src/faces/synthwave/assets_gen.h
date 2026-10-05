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
constexpr int DATE_GLYPH_COUNT = 33;
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

// ---- pogoda: data od lewej, ikona + temperatura po prawej ----
constexpr int DATE_LEFT_X = 22, WX_RIGHT_X = 458;
// ikony (alpha, kolor daty): 0 slonce, 1 noc, 2 chmury, 3 deszcz, 4 snieg, 5 burza, 6 mgla
constexpr int WX_ICON_SIZE = 26, WX_ICON_COUNT = 7;
extern const uint8_t WX_ICONS[WX_ICON_COUNT][WX_ICON_SIZE * WX_ICON_SIZE];
// chmury: bajt = (wypelnienie4 << 4) | obwodka4 (neon magenta)
constexpr int CLOUD_SHAPES = 2;
constexpr int CLOUD_W[CLOUD_SHAPES] = {186, 145};
constexpr int CLOUD_H[CLOUD_SHAPES] = {96, 76};
extern const uint8_t* const CLOUD_DATA[CLOUD_SHAPES];
constexpr uint8_t CLOUD_FILL_R = 40, CLOUD_FILL_G = 14, CLOUD_FILL_B = 60;
constexpr uint8_t CLOUD_RIM_R = 255, CLOUD_RIM_G = 70, CLOUD_RIM_B = 200;

// ---- gwiazdy ----
struct Star { uint16_t x; uint8_t y, phase, size, bright; };
constexpr int STAR_COUNT = 46;
extern const Star STARS[STAR_COUNT];

}  // namespace assets
