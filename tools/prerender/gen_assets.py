"""Prerender zasobow zegarka synthwave (uruchamiane na laptopie).

Jedno zrodlo prawdy dla wygladu sceny: geometria, kolory, cyfry z glow,
atlas daty, tabele slonca, gwiazdy. Wynik:
  src/assets/assets_gen.h / assets_gen.cpp  - dane dla ESP32 (const, we flashu)
  tools/prerender/out/preview.png            - podglad calej klatki (RGB565 +
                                               dithering, jak na ekranie)

Uruchomienie:  python tools/prerender/gen_assets.py
"""

from __future__ import annotations

import random
from dataclasses import dataclass
from pathlib import Path

import numpy as np
from PIL import Image, ImageFilter, ImageFont

ROOT = Path(__file__).resolve().parents[2]
FONTS = Path(__file__).parent / "fonts"
OUT_SRC = ROOT / "src" / "assets"
OUT_PREVIEW = Path(__file__).parent / "out"

# ---------------------------------------------------------------- scena
W, H = 480, 320
HORIZON_Y = 220
DATE_BOTTOM = 40

DIGIT_H = 84            # wysokosc cyfry "0" (ink) w px
DIGIT_CENTER_Y = 98     # srodek pasa cyfr
DIGIT_GAP = 6           # odstep miedzy cyframi w parze HH / MM
COLON_PAD = 10          # odstep cyfra <-> dwukropek
GLOW_PAD = 6            # margines na glow wokol ksztaltu
GLOW_SIGMA = 2.2        # cienki glow (czytelnosc z daleka)
GLOW_STRENGTH = 0.9
GLOW_RGB = (255, 60, 200)     # magenta - jak pionowe linie siatki
CORE_RGB = (255, 255, 255)    # biale wnetrze

DATE_PX = 20            # rozmiar Audiowide
DATE_RGB = (200, 130, 255)
DATE_BASELINE = 29

SUN_CX, SUN_CY, SUN_R = 240, HORIZON_Y, 108
SUN_TOP_RGB = (255, 236, 110)
SUN_MID_RGB = (255, 120, 90)
SUN_BOT_RGB = (255, 40, 160)
SUN_SLICE_SPACING = 12          # okres przeciec = liczba klatek petli
SUN_SLICE_MAX = 9               # grubosc przeciecia przy horyzoncie

SKY_STOPS = [  # (y, rgb) - gradient nieba nad horyzontem
    (0, (6, 0, 18)),
    (120, (24, 4, 52)),
    (190, (60, 10, 92)),
    (219, (150, 30, 140)),
]
GROUND_TOP_RGB = (18, 2, 36)
GROUND_BOT_RGB = (52, 12, 90)

STAR_COUNT = 46
STAR_SEED = 1984

WEEKDAYS = ["NIEDZIELA", "PONIEDZIAŁEK", "WTOREK", "ŚRODA", "CZWARTEK", "PIĄTEK", "SOBOTA"]
DATE_CHARSET = sorted(set("".join(WEEKDAYS) + "0123456789. "))


# ---------------------------------------------------------------- pomocnicze
def rgb565(r: int, g: int, b: int) -> int:
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


def lerp_rgb(a, b, t):
    return tuple(int(round(a[i] + (b[i] - a[i]) * t)) for i in range(3))


def gradient(stops, y):
    for (y0, c0), (y1, c1) in zip(stops, stops[1:]):
        if y <= y1:
            return lerp_rgb(c0, c1, 0 if y1 == y0 else (y - y0) / (y1 - y0))
    return stops[-1][1]


BAYER4 = np.array([[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]])


def dither565(rgb: np.ndarray) -> np.ndarray:
    """RGB888 -> RGB565 (jako RGB888 do podgladu) z ditheringiem Bayer 4x4,
    tym samym co na urzadzeniu."""
    h, w, _ = rgb.shape
    t = np.tile(BAYER4, (h // 4 + 1, w // 4 + 1))[:h, :w][..., None] / 16.0
    steps = np.array([8, 4, 8])
    q = np.floor(np.clip(rgb + t * steps, 0, 255) / steps) * steps
    return np.clip(q, 0, 255).astype(np.uint8)


def orbitron(px: int) -> ImageFont.FreeTypeFont:
    f = ImageFont.truetype(str(FONTS / "Orbitron.ttf"), px)
    f.set_variation_by_axes([700])
    return f


# ---------------------------------------------------------------- cyfry
@dataclass
class Glyph:
    char: str
    w: int
    h: int
    data: np.ndarray  # uint8 index = (core4 << 4) | glow4


def _quant4(a: np.ndarray) -> np.ndarray:
    return np.clip(np.round(a * 15), 0, 15).astype(np.uint8)


def render_digit_glyph(font, ch, cell_w, top_off, cell_h) -> Glyph:
    """Ksztalt cyfry wysrodkowany w komorce cell_w x cell_h (z marginesem glow)."""
    canvas = Image.new("L", (cell_w, cell_h), 0)
    bb = font.getbbox(ch)
    ink_w = bb[2] - bb[0]
    x = (cell_w - ink_w) // 2 - bb[0]
    y = top_off - bb[1]
    from PIL import ImageDraw
    ImageDraw.Draw(canvas).text((x, y), ch, font=font, fill=255)
    core = np.asarray(canvas, dtype=np.float32) / 255.0

    solid = canvas.point(lambda v: 255 if v > 127 else 0).filter(ImageFilter.MaxFilter(3))
    glow = np.asarray(solid.filter(ImageFilter.GaussianBlur(GLOW_SIGMA)), dtype=np.float32) / 255.0
    glow = np.clip(glow * GLOW_STRENGTH * 1.6, 0, 1)

    data = (_quant4(core) << 4) | _quant4(glow)
    return Glyph(ch, cell_w, cell_h, data)


def glyph_lut():
    """256 wpisow: (kolor565, alpha 0-255) dla index=(core4<<4)|glow4."""
    lut = []
    for idx in range(256):
        c = (idx >> 4) / 15.0
        g = (idx & 15) / 15.0
        a = c + (1 - c) * g
        if a <= 0:
            lut.append((0, 0, (0, 0, 0)))
            continue
        rgb = tuple(int(round((CORE_RGB[i] * c + GLOW_RGB[i] * (1 - c) * g) / a)) for i in range(3))
        lut.append((rgb565(*rgb), int(round(a * 255)), rgb))
    return lut


def build_digits():
    probe = orbitron(100)
    b0 = probe.getbbox("0")
    px = round(100 * DIGIT_H / (b0[3] - b0[1]))
    font = orbitron(px)
    b0 = font.getbbox("0")
    digit_h = b0[3] - b0[1]

    ink_w = max(font.getbbox(c)[2] - font.getbbox(c)[0] for c in "0123456789")
    cell_w = ink_w + 2 * GLOW_PAD
    cell_h = digit_h + 2 * GLOW_PAD
    digits = [render_digit_glyph(font, c, cell_w, GLOW_PAD + (font.getbbox(c)[1] - b0[1]), cell_h)
              for c in "0123456789"]

    cb = font.getbbox(":")
    colon_w = (cb[2] - cb[0]) + 2 * GLOW_PAD
    colon = render_digit_glyph(font, ":", colon_w, GLOW_PAD + (cb[1] - b0[1]), cell_h)

    # Uklad HH:MM (komorki nachodza o glow - kazda ma wlasny margines)
    inner = lambda w: w - 2 * GLOW_PAD
    total_ink = 4 * inner(cell_w) + 2 * DIGIT_GAP + 2 * COLON_PAD + inner(colon_w)
    x0 = (W - total_ink) // 2 - GLOW_PAD
    xs = []
    x = x0
    for i in range(4):
        xs.append(x)
        x += inner(cell_w) + (DIGIT_GAP if i in (0, 2) else 0)
        if i == 1:
            colon_x = x + COLON_PAD
            x = colon_x + inner(colon_w) + COLON_PAD
    top = DIGIT_CENTER_Y - cell_h // 2
    return dict(font_px=px, digit_h=digit_h, cell_w=cell_w, cell_h=cell_h, digits=digits,
                colon=colon, xs=xs, colon_x=colon_x, top=top, total_ink=total_ink)


# ---------------------------------------------------------------- data
def build_date_atlas():
    font = ImageFont.truetype(str(FONTS / "Audiowide-Regular.ttf"), DATE_PX)
    ascent, _ = font.getmetrics()
    glyphs = []
    from PIL import ImageDraw
    for ch in DATE_CHARSET:
        adv = int(round(font.getlength(ch)))
        bb = font.getbbox(ch)
        if ch == " " or bb[2] <= bb[0]:
            glyphs.append(dict(cp=ord(ch), w=0, h=0, xoff=0, yoff=0, adv=adv, data=np.zeros(0, np.uint8)))
            continue
        w, h = bb[2] - bb[0], bb[3] - bb[1]
        img = Image.new("L", (w, h), 0)
        ImageDraw.Draw(img).text((-bb[0], -bb[1]), ch, font=font, fill=255)
        glyphs.append(dict(cp=ord(ch), w=w, h=h, xoff=bb[0], yoff=bb[1] - ascent, adv=adv,
                           data=np.asarray(img, np.uint8)))
    return glyphs


# ---------------------------------------------------------------- slonce
def sun_row_color(dy_from_top):
    t = dy_from_top / SUN_R
    return lerp_rgb(SUN_TOP_RGB, SUN_MID_RGB, t / 0.55) if t < 0.55 else \
        lerp_rgb(SUN_MID_RGB, SUN_BOT_RGB, (t - 0.55) / 0.45)


def build_sun(slice_top_y):
    """Polszerokosci wierszy slonca + maski przeciec dla kazdej klatki petli.
    Przeciecia zaczynaja sie ponizej cyfr (slice_top_y), rosna w dol i
    przesuwaja sie o 1 px na klatke; petla = SUN_SLICE_SPACING klatek."""
    rows = []
    for i in range(SUN_R):
        y = SUN_CY - SUN_R + i
        dy = SUN_CY - y - 0.5
        rows.append(int(round((SUN_R * SUN_R - dy * dy) ** 0.5)))
    zone_top = slice_top_y - (SUN_CY - SUN_R)
    zone_len = SUN_R - zone_top
    frames = []
    for f in range(SUN_SLICE_SPACING):
        mask = np.zeros(SUN_R, np.uint8)
        for k in range(-1, zone_len // SUN_SLICE_SPACING + 2):
            pos = k * SUN_SLICE_SPACING + f
            if pos < 0 or pos >= zone_len:
                continue
            thick = 1 + int(SUN_SLICE_MAX * pos / zone_len)
            mask[zone_top + pos: min(SUN_R, zone_top + pos + thick)] = 1
        frames.append(mask)
    return dict(halfw=rows, frames=frames, zone_top=zone_top)


# ---------------------------------------------------------------- gwiazdy
def build_stars(digits_box, sun):
    rnd = random.Random(STAR_SEED)
    stars = []
    while len(stars) < STAR_COUNT:
        x, y = rnd.randrange(4, W - 4), rnd.randrange(DATE_BOTTOM + 2, HORIZON_Y - 20)
        dx, dy = x - SUN_CX, y - SUN_CY
        if dx * dx + dy * dy < (SUN_R + 6) ** 2:
            continue
        x0, y0, x1, y1 = digits_box
        if x0 - 4 <= x <= x1 + 4 and y0 - 4 <= y <= y1 + 4:
            continue
        if any(abs(x - s[0]) < 10 and abs(y - s[1]) < 10 for s in stars):
            continue
        stars.append((x, y, rnd.randrange(256), rnd.choice((1, 1, 1, 2)), rnd.randrange(90, 255)))
    return stars


# ---------------------------------------------------------------- podglad
def render_preview(dg, date_atlas, sun, stars, sun_frame=0, text_date="PIĄTEK  02.10.2026",
                   text_time="23:47"):
    img = np.zeros((H, W, 3), np.float32)
    for y in range(HORIZON_Y):
        img[y, :] = gradient(SKY_STOPS, y)
    for y in range(HORIZON_Y, H):
        img[y, :] = lerp_rgb(GROUND_TOP_RGB, GROUND_BOT_RGB, (y - HORIZON_Y) / (H - HORIZON_Y))

    for (x, y, phase, size, bright) in stars:
        img[y:y + size, x:x + size] = (bright, bright, min(255, bright + 30))

    mask = sun["frames"][sun_frame]
    for i, hw in enumerate(sun["halfw"]):
        if mask[i]:
            continue
        y = SUN_CY - SUN_R + i
        img[y, SUN_CX - hw:SUN_CX + hw] = sun_row_color(i)

    # siatka (statyczna klatka - na urzadzeniu animowana)
    for k in range(1, 14):
        y = HORIZON_Y + int(round((H - 1 - HORIZON_Y) * (k / 13) ** 2.1))
        img[y, :] = (100, 100, 255)
    for lane in range(-9, 10):
        if lane == 0:
            continue
        x0, x1 = SUN_CX + lane * 14, SUN_CX + lane * 120
        for y in range(HORIZON_Y, H):
            t = (y - HORIZON_Y) / (H - 1 - HORIZON_Y)
            x = int(round(x0 + (x1 - x0) * t))
            if 0 <= x < W:
                img[y, x] = (255, 100, 200)
    img[HORIZON_Y, :] = (255, 255, 255)
    img[HORIZON_Y + 1, :] = (180, 200, 255)

    lut = glyph_lut()
    def blit(g, gx, gy):
        for yy in range(g.h):
            for xx in range(g.w):
                c, a, rgb = lut[g.data[yy, xx]]
                if a:
                    p = img[gy + yy, gx + xx]
                    img[gy + yy, gx + xx] = p + (np.array(rgb) - p) * (a / 255)
    for i, ch in enumerate(text_time.replace(":", "")):
        blit(dg["digits"][int(ch)], dg["xs"][i], dg["top"])
    blit(dg["colon"], dg["colon_x"] - GLOW_PAD, dg["top"])

    by_cp = {g["cp"]: g for g in date_atlas}
    tw = sum(by_cp[ord(c)]["adv"] for c in text_date)
    x = (W - tw) // 2
    for c in text_date:
        g = by_cp[ord(c)]
        if g["w"]:
            for yy in range(g["h"]):
                for xx in range(g["w"]):
                    a = g["data"][yy, xx] / 255
                    if a:
                        px, py = x + g["xoff"] + xx, DATE_BASELINE + g["yoff"] + yy
                        img[py, px] += (np.array(DATE_RGB) - img[py, px]) * a
        x += g["adv"]
    return Image.fromarray(dither565(img))


# ---------------------------------------------------------------- eksport C++
def c_array(name, data, ctype="uint8_t", per_line=24):
    vals = [str(int(v)) for v in data]
    lines = [", ".join(vals[i:i + per_line]) for i in range(0, len(vals), per_line)]
    return f"const {ctype} {name}[{len(vals)}] = {{\n  " + ",\n  ".join(lines) + "\n};\n"


def write_sources(dg, date_atlas, sun, stars):
    OUT_SRC.mkdir(parents=True, exist_ok=True)
    lut = glyph_lut()
    sky = [gradient(SKY_STOPS, y) for y in range(HORIZON_Y)]
    ground = [lerp_rgb(GROUND_TOP_RGB, GROUND_BOT_RGB, (y - HORIZON_Y) / (H - HORIZON_Y))
              for y in range(HORIZON_Y, H)]
    sun_rgb = [sun_row_color(i) for i in range(SUN_R)]

    h = f"""#pragma once
// WYGENEROWANE przez tools/prerender/gen_assets.py - nie edytowac recznie.
#include <stdint.h>

namespace assets {{

// ---- uklad ----
constexpr int DIGIT_FONT_PX = {dg['font_px']};
constexpr int DIGIT_CELL_W = {dg['cell_w']};
constexpr int DIGIT_CELL_H = {dg['cell_h']};
constexpr int DIGIT_TOP = {dg['top']};
constexpr int DIGIT_X[4] = {{{', '.join(map(str, dg['xs']))}}};
constexpr int COLON_X = {dg['colon_x'] - GLOW_PAD};
constexpr int COLON_W = {dg['colon'].w};
constexpr int GLOW_PAD = {GLOW_PAD};

// ---- cyfry: index piksela = (core4 << 4) | glow4 -> LUT ----
extern const uint16_t GLYPH_LUT_COLOR[256];
extern const uint8_t GLYPH_LUT_ALPHA[256];
extern const uint8_t DIGIT_DATA[10][DIGIT_CELL_W * DIGIT_CELL_H];
extern const uint8_t COLON_DATA[COLON_W * DIGIT_CELL_H];

// ---- data (Audiowide {DATE_PX}px, alpha 8-bit) ----
struct DateGlyph {{ uint16_t cp; uint8_t w, h; int8_t xoff, yoff; uint8_t adv; uint32_t offset; }};
constexpr int DATE_GLYPH_COUNT = {len(date_atlas)};
constexpr int DATE_BASELINE = {DATE_BASELINE};
constexpr uint8_t DATE_R = {DATE_RGB[0]}, DATE_G = {DATE_RGB[1]}, DATE_B = {DATE_RGB[2]};
extern const DateGlyph DATE_GLYPHS[DATE_GLYPH_COUNT];
extern const uint8_t DATE_ALPHA[];
extern const char* const WEEKDAYS[7];

// ---- tlo: RGB888 na wiersz (dithering na urzadzeniu) ----
extern const uint8_t SKY_RGB[{HORIZON_Y}][3];
extern const uint8_t GROUND_RGB[{H - HORIZON_Y}][3];

// ---- slonce ----
constexpr int SUN_CX = {SUN_CX}, SUN_CY = {SUN_CY}, SUN_R = {SUN_R};
constexpr int SUN_FRAMES = {len(sun['frames'])};
constexpr int SUN_SLICE_ZONE_TOP = {sun['zone_top']};  // wiersz slonca, od ktorego sa przeciecia
extern const uint8_t SUN_HALFW[SUN_R];
extern const uint8_t SUN_RGB[SUN_R][3];
extern const uint8_t SUN_GAP[SUN_FRAMES][SUN_R];  // 1 = przeciecie (tlo nieba)

// ---- gwiazdy ----
struct Star {{ uint16_t x; uint8_t y, phase, size, bright; }};
constexpr int STAR_COUNT = {len(stars)};
extern const Star STARS[STAR_COUNT];

}}  // namespace assets
"""
    cpp = ["// WYGENEROWANE przez tools/prerender/gen_assets.py - nie edytowac recznie.",
           '#include "assets_gen.h"', "", "namespace assets {", ""]
    cpp.append(c_array("GLYPH_LUT_COLOR", [e[0] for e in lut], "uint16_t", 16))
    cpp.append(c_array("GLYPH_LUT_ALPHA", [e[1] for e in lut]))
    cpp.append("const uint8_t DIGIT_DATA[10][DIGIT_CELL_W * DIGIT_CELL_H] = {")
    for g in dg["digits"]:
        cpp.append("  {" + ",".join(map(str, g.data.flatten())) + "},")
    cpp.append("};\n")
    cpp.append(c_array("COLON_DATA", dg["colon"].data.flatten()))

    offset, alpha, entries = 0, [], []
    for g in date_atlas:
        entries.append(f"  {{{g['cp']}, {g['w']}, {g['h']}, {g['xoff']}, {g['yoff']}, {g['adv']}, {offset}}},")
        flat = g["data"].flatten()
        alpha.extend(flat)
        offset += len(flat)
    cpp.append("const DateGlyph DATE_GLYPHS[DATE_GLYPH_COUNT] = {\n" + "\n".join(entries) + "\n};\n")
    cpp.append(c_array("DATE_ALPHA", alpha).replace(f"DATE_ALPHA[{len(alpha)}]", "DATE_ALPHA[]"))
    cpp.append("const char* const WEEKDAYS[7] = {" + ", ".join(f'"{d}"' for d in WEEKDAYS) + "};\n")

    rows3 = lambda name, rows: f"const uint8_t {name}[{len(rows)}][3] = {{" + \
        ", ".join("{%d,%d,%d}" % r for r in rows) + "};\n"
    cpp.append(rows3("SKY_RGB", sky))
    cpp.append(rows3("GROUND_RGB", ground))
    cpp.append(c_array("SUN_HALFW", sun["halfw"]))
    cpp.append(rows3("SUN_RGB", sun_rgb))
    cpp.append("const uint8_t SUN_GAP[SUN_FRAMES][SUN_R] = {")
    for m in sun["frames"]:
        cpp.append("  {" + ",".join(map(str, m)) + "},")
    cpp.append("};\n")
    cpp.append("const Star STARS[STAR_COUNT] = {\n" +
               "\n".join(f"  {{{x}, {y}, {p}, {s}, {b}}}," for x, y, p, s, b in stars) + "\n};\n")
    cpp.append("}  // namespace assets")

    (OUT_SRC / "assets_gen.h").write_text(h, encoding="utf-8")
    (OUT_SRC / "assets_gen.cpp").write_text("\n".join(cpp) + "\n", encoding="utf-8")
    return sum(g.data.size for g in dg["digits"]) + dg["colon"].data.size + len(alpha)


def main():
    dg = build_digits()
    digits_box = (dg["xs"][0], dg["top"], dg["xs"][3] + dg["cell_w"], dg["top"] + dg["cell_h"])
    # przeciecia slonca dopiero ponizej cyfr (z glow) - animacja nie dotyka cyfr
    sun = build_sun(slice_top_y=digits_box[3] + 2)
    stars = build_stars(digits_box, sun)
    date_atlas = build_date_atlas()

    nbytes = write_sources(dg, date_atlas, sun, stars)

    OUT_PREVIEW.mkdir(exist_ok=True)
    prev = render_preview(dg, date_atlas, sun, stars)
    prev.save(OUT_PREVIEW / "preview.png")
    prev.resize((W * 2, H * 2), Image.NEAREST).save(OUT_PREVIEW / "preview_x2.png")
    frames = [render_preview(dg, date_atlas, sun, stars, sun_frame=f, text_time="00:00")
              for f in range(0, len(sun["frames"]), 3)]
    strip = Image.new("RGB", (W, H * len(frames)))
    for i, f in enumerate(frames):
        strip.paste(f, (0, i * H))
    strip.save(OUT_PREVIEW / "sun_frames.png")

    print(f"Orbitron {dg['font_px']}px -> cyfra '0' {dg['digit_h']} px wysokosci")
    print(f"komorka cyfry {dg['cell_w']}x{dg['cell_h']} (z glow {GLOW_PAD}px), "
          f"HH:MM ink {dg['total_ink']} px, x od {dg['xs'][0] + GLOW_PAD} do "
          f"{dg['xs'][3] + dg['cell_w'] - GLOW_PAD}, y {dg['top'] + GLOW_PAD}-{dg['top'] + dg['cell_h'] - GLOW_PAD}")
    print(f"slonce r={SUN_R}, gora y={SUN_CY - SUN_R}, przeciecia od y={SUN_CY - SUN_R + sun['zone_top']}")
    print(f"zasoby graficzne: {nbytes / 1024:.1f} KB we flashu")


if __name__ == "__main__":
    main()
