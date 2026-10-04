"""Zasoby tarczy "portal": mapa biegunowa portalu (promien znormalizowany +
kat 16-bit dla kazdego piksela animowanego prostokata), cyfry Creepster
z obrysem, data Audiowide, lista gwiazd. Podglad: out/preview.png.

  python tools/prerender/portal/gen_assets.py
"""

from __future__ import annotations

import sys
from pathlib import Path

import numpy as np

HERE = Path(__file__).parent
sys.path.insert(0, str(HERE.parent))
import common_hd as hd  # noqa: E402

ROOT = HERE.parents[2]
OUT_SRC = ROOT / "src" / "faces" / "portal"
W, H = hd.W, hd.H

CX, CY, RX, RY = 240, 140, 150, 112
R_ONE = 200          # r8 = 200 -> krawedz elipsy
ANIM = 1.1           # animowany prostokat: elipsa x1.1 (krawedz faluje +-8%)
STAR_MIN_RN = 1.18


def main():
    ax0, ax1 = int(CX - RX * ANIM) - 2, int(CX + RX * ANIM) + 3
    ay0, ay1 = int(CY - RY * ANIM) - 2, int(CY + RY * ANIM) + 3
    ys, xs = np.mgrid[ay0:ay1, ax0:ax1].astype(np.float32)
    dx, dy = (xs + 0.5 - CX) / RX, (ys + 0.5 - CY) / RY
    rn = np.hypot(dx, dy)
    r8 = np.clip(np.round(rn * R_ONE), 0, 255).astype(np.uint8)
    ang = (np.round(np.arctan2(dy * RY, dx * RX) / (2 * np.pi) * 65536) % 65536).astype(np.uint16)
    pmap = np.stack([r8.astype(np.uint16), ang], -1)  # r8, kat

    clock = hd.build_clock(hd.font("Creepster.ttf", 118), "outline", cy=CY, pad=8, gap=6, colon_gap=4, stroke=6)
    date = hd.build_date(hd.font("Audiowide-Regular.ttf", 20), "outline", baseline=298, stroke=3)

    rng = np.random.default_rng(1)
    stars = []
    while len(stars) < 80:
        x, y = int(rng.integers(0, W)), int(rng.integers(0, H - 40))
        if np.hypot((x - CX) / RX, (y - CY) / RY) > STAR_MIN_RN:
            stars.append((x, y, int(90 + rng.random() * 160)))

    extra_h = f"""
constexpr int CX = {CX}, CY = {CY}, RX = {RX}, RY = {RY}, R_ONE = {R_ONE};
constexpr int MAP_X = {ax0}, MAP_Y = {ay0}, MAP_W = {ax1 - ax0}, MAP_H = {ay1 - ay0};
// [r8, kat16] na piksel; r8 = R_ONE na krawedzi elipsy
extern const uint16_t PORTAL_MAP[MAP_W * MAP_H][2];
struct Star {{ int16_t x, y; uint8_t b; }};
constexpr int STAR_COUNT = {len(stars)};
extern const Star STARS[STAR_COUNT];
"""
    extra_cpp = hd.c_array("PORTAL_MAP", pmap, "uint16_t", dims="[MAP_W * MAP_H][2]")
    extra_cpp += "const Star STARS[STAR_COUNT] = {" + ", ".join(f"{{{x}, {y}, {b}}}" for x, y, b in stars) + "};\n"
    hd.write_assets(OUT_SRC, "tools/prerender/portal/gen_assets.py", clock, date, extra_h, extra_cpp)
    print(f"mapa {ax1 - ax0}x{ay1 - ay0} @({ax0},{ay0}), cyfry {clock['cell_w']}x{clock['cell_h']} top {clock['top']}"
          f" x={clock['xs']}, data {date['top']}..{date['bottom']}")


if __name__ == "__main__":
    main()
