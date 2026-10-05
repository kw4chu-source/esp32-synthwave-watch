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

// ---- szyld pogody (dawny hologram; pas skanowania zostaje) ----
constexpr int HOLO_X0 = 430, HOLO_Y0 = 52, HOLO_X1 = 474, HOLO_Y1 = 168;
// ikony: 0 slonce, 1 noc, 2 chmury, 3 deszcz, 4 snieg, 5 burza, 6 mgla (format jak cyfry)
constexpr int WX_ICON_SIZE = 40, WX_ICON_COUNT = 7;
constexpr int WX_ICON_X = 432, WX_ICON_Y = 58;
extern const uint8_t WX_ICONS[WX_ICON_COUNT][WX_ICON_SIZE * WX_ICON_SIZE];
// temperatura (Tilt Neon 30px): cyfry, minus, stopien
constexpr int WX_TEMP_GLYPH_COUNT = 12;
constexpr int WX_TEMP_CENTER_X = 452, WX_TEMP_BASELINE = 135;
extern const DateGlyph WX_TEMP_GLYPHS[WX_TEMP_GLYPH_COUNT];
extern const uint8_t WX_TEMP_DATA[];
extern const uint8_t LUT_WX[256][3];

}  // namespace assets
