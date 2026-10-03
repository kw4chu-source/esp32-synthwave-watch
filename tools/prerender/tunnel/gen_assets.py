"""Zasoby tarczy "tunnel" (cyberpunk): neonowe cyfry Rajdhani (cyan), tablica
z zolta ramka, data (magenta). Tunel i smugi rysowane na ESP32 na zywo.

Format neonow jak w neon_rain: 1 B/piksel = (core4 << 4) | glow4, swiatlo
dodawane z LUT RGB888.

  python tools/prerender/tunnel/gen_assets.py
"""

from __future__ import annotations

import sys
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFont

HERE = Path(__file__).parent
sys.path.insert(0, str(HERE.parent / "neon_rain"))
import gen_assets as nr  # noqa: E402  (neon_glyph, neon_lut, glow_intensity, q4)

ROOT = HERE.parents[2]
FONTS = HERE.parent / "fonts"
OUT_SRC = ROOT / "src" / "faces" / "tunnel"
W, H = 480, 320

PLATE = (110, 100, 370, 200)
PLATE_R = 14
PLATE_PAD = 8
DIGIT_PX = 104
DIGIT_CY = 150
GLOW_PAD = 12
DATE_PX = 20
DATE_TOP = 208

CYAN, CYAN_CORE = (0, 240, 255), (230, 255, 255)
YELLOW, YELLOW_CORE = (252, 238, 10), (255, 255, 220)
MAGENTA, MAGENTA_CORE = (255, 40, 200), (255, 210, 245)
DIGIT_RADII = ((8, 0.6), (3, 0.9))
PLATE_RADII = ((4, 0.5),)
DATE_RADII = ((3, 0.5),)

WEEKDAYS = ["NIEDZIELA", "PONIEDZIAŁEK", "WTOREK", "ŚRODA", "CZWARTEK", "PIĄTEK", "SOBOTA"]
DATE_CHARSET = sorted(set("".join(WEEKDAYS) + "0123456789. "))


def font(px):
    return ImageFont.truetype(str(FONTS / "Rajdhani-Bold.ttf"), px)


def c_array(name, data, ctype="uint8_t", dims=None, per_line=32):
    vals = [str(int(v)) for v in np.asarray(data).flatten()]
    lines = [", ".join(vals[i:i + per_line]) for i in range(0, len(vals), per_line)]
    return f"const {ctype} {name}{dims or f'[{len(vals)}]'} = {{\n  " + ",\n  ".join(lines) + "\n};\n"


def main():
    OUT_SRC.mkdir(parents=True, exist_ok=True)
    f = font(DIGIT_PX)
    b8 = f.getbbox("8")
    ink_h = b8[3] - b8[1]
    ink_w = max(f.getbbox(c)[2] - f.getbbox(c)[0] for c in "0123456789")
    cell_w, cell_h = ink_w + 2 * GLOW_PAD, ink_h + 2 * GLOW_PAD
    digits = []
    for c in "0123456789":
        bb = f.getbbox(c)
        digits.append(nr.neon_glyph(f, c, cell_w, cell_h, (cell_w - (bb[2] - bb[0])) // 2 - bb[0], GLOW_PAD - b8[1], DIGIT_RADII))
    cb = f.getbbox(":")
    colon_w = (cb[2] - cb[0]) + 2 * GLOW_PAD
    colon = nr.neon_glyph(f, ":", colon_w, cell_h, GLOW_PAD - cb[0], GLOW_PAD - b8[1], DIGIT_RADII)
    gap = 8
    colon_gap = 6
    total = 4 * ink_w + 2 * gap + 2 * colon_gap + (cb[2] - cb[0])
    x = (W - total) // 2
    xs = []
    for i in range(4):
        xs.append(x - GLOW_PAD)
        x += ink_w + (gap if i in (0, 2) else 0)
        if i == 1:
            x += colon_gap
            colon_x = x - GLOW_PAD
            x += (cb[2] - cb[0]) + colon_gap
    top = DIGIT_CY - ink_h // 2 - GLOW_PAD

    # tablica: wypelnienie (maska) + swiecaca zolta ramka
    x0, y0, x1, y1 = PLATE
    pw, ph = x1 - x0 + 2 * PLATE_PAD, y1 - y0 + 2 * PLATE_PAD
    fill_img = Image.new("L", (pw, ph), 0)
    ImageDraw.Draw(fill_img).rounded_rectangle([PLATE_PAD, PLATE_PAD, pw - PLATE_PAD - 1, ph - PLATE_PAD - 1],
                                               radius=PLATE_R, fill=255)
    plate_fill = (np.asarray(fill_img) > 127).astype(np.uint8)
    border = Image.new("L", (pw, ph), 0)
    ImageDraw.Draw(border).rounded_rectangle([PLATE_PAD, PLATE_PAD, pw - PLATE_PAD - 1, ph - PLATE_PAD - 1],
                                             radius=PLATE_R, outline=255, width=2)
    core = np.asarray(border, np.float32) / 255
    imax = sum(k for _, k in PLATE_RADII) * 1.6
    g = np.clip(nr.glow_intensity(core, PLATE_RADII) / imax, 0, 1)
    plate_glow = (nr.q4(core) << 4) | nr.q4(np.sqrt(g))

    # data
    fd = font(DATE_PX)
    asc, _ = fd.getmetrics()
    glyphs = []
    for ch in DATE_CHARSET:
        adv = int(round(fd.getlength(ch)))
        bb = fd.getbbox(ch)
        if ch == " " or bb[2] <= bb[0]:
            glyphs.append(dict(cp=32, w=0, h=0, xoff=0, yoff=0, adv=adv, data=np.zeros(0, np.uint8)))
            continue
        p = 4
        w_, h_ = bb[2] - bb[0] + 2 * p, bb[3] - bb[1] + 2 * p
        glyphs.append(dict(cp=ord(ch), w=w_, h=h_, xoff=bb[0] - p, yoff=bb[1] - asc - p, adv=adv,
                           data=nr.neon_glyph(fd, ch, w_, h_, p - bb[0], p - bb[1], DATE_RADII)))

    luts = [("CYAN", nr.neon_lut(CYAN, CYAN_CORE, DIGIT_RADII)), ("YELLOW", nr.neon_lut(YELLOW, YELLOW_CORE, PLATE_RADII)),
            ("MAGENTA", nr.neon_lut(MAGENTA, MAGENTA_CORE, DATE_RADII))]

    hdr = f"""#pragma once
// WYGENEROWANE przez tools/prerender/tunnel/gen_assets.py - nie edytowac recznie.
#include <stdint.h>

namespace assets {{

constexpr int DIGIT_CELL_W = {cell_w}, DIGIT_CELL_H = {cell_h}, DIGIT_TOP = {top};
constexpr int DIGIT_X[4] = {{{', '.join(map(str, xs))}}};
constexpr int COLON_X = {colon_x}, COLON_W = {colon_w};
extern const uint8_t DIGIT_DATA[10][DIGIT_CELL_W * DIGIT_CELL_H];
extern const uint8_t COLON_DATA[COLON_W * DIGIT_CELL_H];

// tablica pod cyframi: maska wypelnienia + zolta ramka (neon)
constexpr int PLATE_X = {x0 - PLATE_PAD}, PLATE_Y = {y0 - PLATE_PAD}, PLATE_W = {pw}, PLATE_H = {ph};
extern const uint8_t PLATE_FILL[PLATE_W * PLATE_H];
extern const uint8_t PLATE_GLOW[PLATE_W * PLATE_H];

struct DateGlyph {{ uint16_t cp; uint8_t w, h; int8_t xoff, yoff; uint8_t adv; uint32_t offset; }};
constexpr int DATE_BASELINE = {DATE_TOP + asc}, DATE_CENTER_X = {W // 2};
constexpr int DATE_GLYPH_COUNT = {len(glyphs)};
extern const DateGlyph DATE_GLYPHS[DATE_GLYPH_COUNT];
extern const uint8_t DATE_DATA[];
extern const char* const WEEKDAYS[7];

extern const uint8_t LUT_CYAN[256][3];
extern const uint8_t LUT_YELLOW[256][3];
extern const uint8_t LUT_MAGENTA[256][3];

}}  // namespace assets
"""
    cpp = ["// WYGENEROWANE przez tools/prerender/tunnel/gen_assets.py - nie edytowac recznie.",
           '#include "assets_gen.h"', "", "namespace assets {", ""]
    cpp.append("const uint8_t DIGIT_DATA[10][DIGIT_CELL_W * DIGIT_CELL_H] = {")
    cpp += ["  {" + ",".join(map(str, d.flatten())) + "}," for d in digits] + ["};\n"]
    cpp.append(c_array("COLON_DATA", colon))
    cpp.append(c_array("PLATE_FILL", plate_fill))
    cpp.append(c_array("PLATE_GLOW", plate_glow))
    off, data, ent = 0, [], []
    for g_ in glyphs:
        ent.append(f"  {{{g_['cp']}, {g_['w']}, {g_['h']}, {g_['xoff']}, {g_['yoff']}, {g_['adv']}, {off}}},")
        fl = np.asarray(g_["data"]).flatten()
        data.extend(fl)
        off += len(fl)
    cpp.append("const DateGlyph DATE_GLYPHS[DATE_GLYPH_COUNT] = {\n" + "\n".join(ent) + "\n};\n")
    cpp.append(c_array("DATE_DATA", data, dims="[]"))
    cpp.append("const char* const WEEKDAYS[7] = {" + ", ".join(f'"{d}"' for d in WEEKDAYS) + "};\n")
    for name, lut in luts:
        cpp.append(c_array(f"LUT_{name}", [v for e in lut for v in e], dims="[256][3]"))
    cpp.append("}  // namespace assets")
    (OUT_SRC / "assets_gen.h").write_text(hdr, encoding="utf-8")
    (OUT_SRC / "assets_gen.cpp").write_text("\n".join(cpp) + "\n", encoding="utf-8")
    print(f"cyfry {cell_w}x{cell_h} x={xs} top={top}, dwukropek x={colon_x}, tablica {pw}x{ph}")


if __name__ == "__main__":
    main()
