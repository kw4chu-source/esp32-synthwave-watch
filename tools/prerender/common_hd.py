"""Wspolne zasoby tarcz pelnej rozdzielczosci (src/faces/common_hd):
cyfry HH:MM i data jako glify 1 B/piksel = (hi4 << 4) | lo4.

  neon     hi = rdzen, lo = poswiata (sqrt)  -> swiatlo dodawane z LUT RGB888
  outline  hi = wypelnienie, lo = obrys       -> dwa kolory nakladane alfa

Uzycie w gen_assets.py tarczy:
  clock = build_clock(font, "neon", radii=..., cy=..)
  date = build_date(font, "outline", stroke=3)
  write_assets(out_dir, "portal", clock, date, extra_h, extra_cpp)
"""

from __future__ import annotations

import sys
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFont

HERE = Path(__file__).parent
sys.path.insert(0, str(HERE / "neon_rain"))
import gen_assets as nr  # noqa: E402

FONTS = HERE / "fonts"
W, H = 480, 320
WEEKDAYS = ["NIEDZIELA", "PONIEDZIAŁEK", "WTOREK", "ŚRODA", "CZWARTEK", "PIĄTEK", "SOBOTA"]
DATE_CHARSET = sorted(set("".join(WEEKDAYS) + "0123456789. "))

glow_intensity, neon_lut, q4 = nr.glow_intensity, nr.neon_lut, nr.q4


def font(name, px):
    return ImageFont.truetype(str(FONTS / name), px)


def c_array(name, data, ctype="uint8_t", dims=None, per_line=40):
    vals = [str(int(v)) for v in np.asarray(data).flatten()]
    lines = [",".join(vals[i:i + per_line]) for i in range(0, len(vals), per_line)]
    return f"const {ctype} {name}{dims or f'[{len(vals)}]'} = {{\n  " + ",\n  ".join(lines) + "\n};\n"


def glyph_cell(fnt, ch, w, h, xo, yo, kind, radii=None, stroke=0):
    if kind == "neon":
        return nr.neon_glyph(fnt, ch, w, h, xo, yo, radii)
    fill = Image.new("L", (w, h), 0)
    ImageDraw.Draw(fill).text((xo, yo), ch, font=fnt, fill=255)
    sil = Image.new("L", (w, h), 0)
    ImageDraw.Draw(sil).text((xo, yo), ch, font=fnt, fill=255, stroke_width=stroke, stroke_fill=255)
    return (q4(np.asarray(fill, np.float32) / 255) << 4) | q4(np.asarray(sil, np.float32) / 255)


def build_clock(fnt, kind, cy, cx=W // 2, pad=12, gap=8, colon_gap=6, radii=None, stroke=0):
    b8 = fnt.getbbox("8")
    ink_h = b8[3] - b8[1]
    ink_w = max(fnt.getbbox(c)[2] - fnt.getbbox(c)[0] for c in "0123456789")
    cell_w, cell_h = ink_w + 2 * pad, ink_h + 2 * pad
    digits = []
    for c in "0123456789":
        bb = fnt.getbbox(c)
        digits.append(glyph_cell(fnt, c, cell_w, cell_h, (cell_w - (bb[2] - bb[0])) // 2 - bb[0], pad - b8[1], kind,
                                 radii, stroke))
    cb = fnt.getbbox(":")
    cwid = cb[2] - cb[0]
    colon_w = cwid + 2 * pad
    colon = glyph_cell(fnt, ":", colon_w, cell_h, pad - cb[0], pad - b8[1], kind, radii, stroke)
    total = 4 * ink_w + 2 * gap + 2 * colon_gap + cwid
    x = cx - total // 2
    xs = []
    colon_x = 0
    for i in range(4):
        xs.append(x - pad)
        x += ink_w + (gap if i in (0, 2) else 0)
        if i == 1:
            x += colon_gap
            colon_x = x - pad
            x += cwid + colon_gap
    return dict(cell_w=cell_w, cell_h=cell_h, top=cy - ink_h // 2 - pad, xs=xs, colon_x=colon_x, colon_w=colon_w,
                digits=digits, colon=colon)


def build_date(fnt, kind, baseline, radii=None, stroke=0, pad=4):
    asc, _ = fnt.getmetrics()
    glyphs = []
    for ch in DATE_CHARSET:
        adv = int(round(fnt.getlength(ch)))
        bb = fnt.getbbox(ch)
        if ch == " " or bb[2] <= bb[0]:
            glyphs.append(dict(cp=32, w=0, h=0, xoff=0, yoff=0, adv=adv, data=np.zeros(0, np.uint8)))
            continue
        p = pad + stroke
        w_, h_ = bb[2] - bb[0] + 2 * p, bb[3] - bb[1] + 2 * p
        glyphs.append(dict(cp=ord(ch), w=w_, h=h_, xoff=bb[0] - p, yoff=bb[1] - asc - p, adv=adv,
                           data=glyph_cell(fnt, ch, w_, h_, p - bb[0], p - bb[1], kind, radii, stroke)))
    top = baseline - asc - pad - stroke
    bottom = baseline + max(g["yoff"] + g["h"] for g in glyphs)
    return dict(glyphs=glyphs, baseline=baseline, top=top, bottom=bottom)


def write_assets(out_dir: Path, tool: str, clock, date, extra_h="", extra_cpp=""):
    out_dir.mkdir(parents=True, exist_ok=True)
    c = clock
    hdr = f"""#pragma once
// WYGENEROWANE przez {tool} - nie edytowac recznie.
#include <stdint.h>

#include "faces/common_hd/hd.h"

namespace assets {{

constexpr int DIGIT_CELL_W = {c['cell_w']}, DIGIT_CELL_H = {c['cell_h']}, DIGIT_TOP = {c['top']};
constexpr int DIGIT_X[4] = {{{', '.join(map(str, c['xs']))}}};
constexpr int COLON_X = {c['colon_x']}, COLON_W = {c['colon_w']};
extern const uint8_t DIGIT_DATA[10][DIGIT_CELL_W * DIGIT_CELL_H];
extern const uint8_t COLON_DATA[COLON_W * DIGIT_CELL_H];

constexpr int DATE_BASELINE = {date['baseline']}, DATE_TOP = {date['top']}, DATE_BOTTOM = {date['bottom']};
constexpr int DATE_GLYPH_COUNT = {len(date['glyphs'])};
extern const hd::Glyph DATE_GLYPHS[DATE_GLYPH_COUNT];
extern const uint8_t DATE_DATA[];
extern const char* const WEEKDAYS[7];
{extra_h}
}}  // namespace assets
"""
    cpp = [f"// WYGENEROWANE przez {tool} - nie edytowac recznie.", '#include "assets_gen.h"', "",
           "namespace assets {", "", "const uint8_t DIGIT_DATA[10][DIGIT_CELL_W * DIGIT_CELL_H] = {"]
    cpp += ["  {" + ",".join(map(str, d.flatten())) + "}," for d in c["digits"]] + ["};\n"]
    cpp.append(c_array("COLON_DATA", c["colon"]))
    off, data, ent = 0, [], []
    for g in date["glyphs"]:
        ent.append(f"  {{{g['cp']}, {g['w']}, {g['h']}, {g['xoff']}, {g['yoff']}, {g['adv']}, {off}}},")
        fl = np.asarray(g["data"]).flatten()
        data.extend(fl)
        off += len(fl)
    cpp.append("const hd::Glyph DATE_GLYPHS[DATE_GLYPH_COUNT] = {\n" + "\n".join(ent) + "\n};\n")
    cpp.append(c_array("DATE_DATA", data, dims="[]"))
    cpp.append("const char* const WEEKDAYS[7] = {" + ", ".join(f'"{d}"' for d in WEEKDAYS) + "};\n")
    cpp.append(extra_cpp)
    cpp.append("}  // namespace assets")
    (out_dir / "assets_gen.h").write_text(hdr, encoding="utf-8")
    (out_dir / "assets_gen.cpp").write_text("\n".join(cpp) + "\n", encoding="utf-8")


def lut_array(name, lut):
    return c_array(name, [v for e in lut for v in e], dims="[256][3]")
