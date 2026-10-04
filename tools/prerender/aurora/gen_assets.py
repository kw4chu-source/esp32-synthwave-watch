"""Zasoby tarczy "aurora": cyfry TiltNeon (neon zielony z cieniem), data,
profile gor (dwa plany), szum promieni zorzy, gwiazdy nad gorami.
Sama zorza i odbicie liczone na ESP32 na zywo.

  python tools/prerender/aurora/gen_assets.py
"""

from __future__ import annotations

import sys
from pathlib import Path

import numpy as np
from PIL import Image

HERE = Path(__file__).parent
sys.path.insert(0, str(HERE.parent))
import common_hd as hd  # noqa: E402

ROOT = HERE.parents[2]
OUT_SRC = ROOT / "src" / "faces" / "aurora"
W, H = hd.W, hd.H
HORIZON = 222


def main():
    xs = np.arange(W)
    ridge = (HORIZON - 18 - 30 * np.abs(np.sin(xs / 70 + 0.5)) - 18 * np.abs(np.sin(xs / 23 + 2))
             - 6 * np.sin(xs / 7)).astype(int)
    ridge2 = (HORIZON - 6 - 14 * np.abs(np.sin(xs / 45 + 1.3)) - 5 * np.sin(xs / 11)).astype(int)
    rng = np.random.default_rng(3)
    rays = np.asarray(Image.fromarray((rng.random((2, 120)) * 255).astype(np.uint8)).resize((W, 1), Image.BICUBIC),
                      np.uint8)[0]
    srng = np.random.default_rng(7)
    stars = []
    while len(stars) < 150:
        x, y = int(srng.integers(0, W)), int(srng.integers(4, HORIZON))
        if y < ridge[x] - 2:
            stars.append((x, y, int(70 + srng.random() ** 2 * 170), int(srng.integers(0, 256))))

    radii = ((8, 0.6), (3, 0.9))
    lut = hd.neon_lut((60, 210, 150), (110, 235, 180), radii)
    lut_date = hd.neon_lut((60, 200, 150), (120, 230, 185), ((3, 0.5),))
    clock = hd.build_clock(hd.font("TiltNeon.ttf", 108), "neon", cy=64, radii=radii)
    date = hd.build_date(hd.font("Rajdhani-Bold.ttf", 22), "neon", baseline=309, radii=((3, 0.5),))

    extra_h = f"""
constexpr int HORIZON = {HORIZON};
extern const int16_t RIDGE[480];
extern const int16_t RIDGE2[480];
extern const uint8_t RAYS[480];
struct Star {{ int16_t x, y; uint8_t b, phase; }};
constexpr int STAR_COUNT = {len(stars)};
extern const Star STARS[STAR_COUNT];
extern const uint8_t LUT_CLOCK[256][3];
extern const uint8_t LUT_DATE[256][3];
"""
    extra_cpp = hd.c_array("RIDGE", ridge, "int16_t", dims="[480]")
    extra_cpp += hd.c_array("RIDGE2", ridge2, "int16_t", dims="[480]")
    extra_cpp += hd.c_array("RAYS", rays, dims="[480]")
    extra_cpp += "const Star STARS[STAR_COUNT] = {" + ", ".join(f"{{{x}, {y}, {b}, {p}}}" for x, y, b, p in stars) + "};\n"
    extra_cpp += hd.lut_array("LUT_CLOCK", lut)
    extra_cpp += hd.lut_array("LUT_DATE", lut_date)
    hd.write_assets(OUT_SRC, "tools/prerender/aurora/gen_assets.py", clock, date, extra_h, extra_cpp)
    print(f"cyfry top {clock['top']} h {clock['cell_h']} x={clock['xs']}; data {date['top']}..{date['bottom']}")


if __name__ == "__main__":
    main()
