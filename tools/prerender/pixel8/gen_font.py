"""Czcionka pikselowa Press Start 2P (8 px) dla tarcz 8-bit -> bitmapy w C++.

Kazdy znak: komorka 8 x 12 (2 wiersze nad wersalikami na akcenty, 2 pod
na ogonki), wiersz = bajt (bit 7 = lewy piksel), szerokosc przesuniecia 8.

  python tools/prerender/pixel8/gen_font.py -> src/faces/common_8bit/pixel_font_gen.h
"""

from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[3]
FONT = Path(__file__).resolve().parents[1] / "fonts" / "PressStart2P-Regular.ttf"
OUT = ROOT / "src" / "faces" / "common_8bit" / "pixel_font_gen.h"

CHARSET = " 0123456789.:/+-%°ABCDEFGHIJKLMNOPQRSTUVWXYZĄĆĘŁŃÓŚŹŻ"
CELL_H, TOP = 12, 2


def main():
    f = ImageFont.truetype(str(FONT), 8)
    rows = []
    for ch in CHARSET:
        img = Image.new("1", (8, CELL_H), 0)
        d = ImageDraw.Draw(img)
        d.fontmode = "1"
        d.text((0, TOP), ch, font=f, fill=1)
        a = np.asarray(img, bool)
        rows.append([int("".join("1" if v else "0" for v in r), 2) for r in a])
    body = "\n".join(f"  {{{', '.join(f'0x{v:02X}' for v in r)}}},  // {ch!r}" for ch, r in zip(CHARSET, rows))
    cps = ", ".join(str(ord(c)) for c in CHARSET)
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text(f"""#pragma once
// WYGENEROWANE przez tools/prerender/pixel8/gen_font.py - nie edytowac recznie.
// Press Start 2P 8 px: komorka 8x{CELL_H}, wersaliki zaczynaja sie w wierszu {TOP}.
#include <stdint.h>

namespace pix8 {{
constexpr int FONT_CELL_H = {CELL_H}, FONT_TOP = {TOP}, FONT_COUNT = {len(CHARSET)};
constexpr uint16_t FONT_CP[FONT_COUNT] = {{{cps}}};
constexpr uint8_t FONT_ROWS[FONT_COUNT][FONT_CELL_H] = {{
{body}
}};
}}  // namespace pix8
""", encoding="utf-8")
    print(f"{len(CHARSET)} znakow -> {OUT}")


if __name__ == "__main__":
    main()
