#pragma once
// WYGENEROWANE przez tools/prerender/neon_rain/gen_assets.py - nie edytowac recznie.
#include <stdint.h>

namespace assets {

constexpr int STREET_Y = 286;

// ---- tlo (RGB565, natywna kolejnosc bajtow) ----
extern const uint16_t BG[320 * 480];

// ---- cyfry-neony: index = (core4 << 4) | glow4 (glow nieliniowo) -> LUT ----
constexpr int DIGIT_CELL_W = 106;
constexpr int DIGIT_CELL_H = 120;
constexpr int DIGIT_TOP = 33;
constexpr int DIGIT_X[4] = {53, 131, 243, 321};
constexpr int COLON_X = 209;
constexpr int COLON_W = 62;
extern const uint8_t DIGIT_DATA[10][DIGIT_CELL_W * DIGIT_CELL_H];
extern const uint8_t COLON_DATA[COLON_W * DIGIT_CELL_H];
// LUT: RGB888 dodawane do tla z nasyceniem (neon = swiatlo)
extern const uint8_t LUT_ON[256][3];
extern const uint8_t LUT_DIM[256][3];  // przygaszona rurka (migotanie)

// ---- data (Tilt Neon 19px, ten sam format co cyfry) ----
struct DateGlyph { uint16_t cp; uint8_t w, h; int8_t xoff, yoff; uint8_t adv; uint32_t offset; };
constexpr int DATE_GLYPH_COUNT = 31;
constexpr int DATE_BASELINE = 169;
constexpr int DATE_CENTER_X = 240;
constexpr int DATE_BAND_TOP = 143;
constexpr int DATE_BAND_BOTTOM = 179;
extern const DateGlyph DATE_GLYPHS[DATE_GLYPH_COUNT];
extern const uint8_t DATE_DATA[];
extern const uint8_t LUT_DATE[256][3];
extern const char* const WEEKDAYS[7];

// ---- okna, ktore gasna/zapalaja sie ----
struct Window { uint16_t x, y; uint16_t lit, dark; };
constexpr int WINDOW_COUNT = 48;
constexpr int WINDOW_W = 3, WINDOW_H = 4;
extern const Window WINDOWS[WINDOW_COUNT];

// ---- latajace auto ----
constexpr int CAR_W = 64, CAR_H = 22;
constexpr int CAR_Y = 7;
extern const uint16_t CAR_COLOR[CAR_W * CAR_H];
extern const uint8_t CAR_ALPHA[CAR_W * CAR_H];

// ---- hologram ----
constexpr int HOLO_X0 = 430, HOLO_Y0 = 52, HOLO_X1 = 474, HOLO_Y1 = 168;

}  // namespace assets
