"""Galeria do README: zbiera rendery tarcz z generatorow do docs/img.

  python tools/prerender/gallery.py

Wymaga wczesniej wygenerowanych makiet (fun_options, cyber_options,
pixel_options). Tarcze z pogoda renderowane na nowo (weather_options).
"""

from __future__ import annotations

import shutil
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

HERE = Path(__file__).parent
ROOT = HERE.parents[1]
DOCS = ROOT / "docs" / "img"
FONTS = HERE / "fonts"
W, H = 480, 320

sys.path.insert(0, str(HERE / "weather_options"))
import mockups as wx  # noqa: E402

wx.label = lambda img, txt: img  # bez podpisow wariantu


def gif_frame(path: Path, i: int) -> Image.Image:
    im = Image.open(path)
    im.seek(i)
    return im.convert("RGB")


def crop_first(path: Path) -> Image.Image:
    return Image.open(path).convert("RGB").crop((0, 0, W, H))


def tiles() -> list[tuple[str, str, Image.Image]]:
    import numpy as np  # noqa: F401
    nr, sw = wx.nr, wx.sw
    bg, _ = nr.build_background()
    dg = nr.build_digits()
    lut_on = nr.neon_lut(nr.TUBE_RGB, nr.TUBE_CORE, nr.DIGIT_RADII)
    date_glyphs, ascent = nr.build_date_atlas()
    neon = wx.neon_frame("deszcz", (bg, dg, lut_on, date_glyphs, nr.SIGN_BOX[3] - 26 + ascent))
    sdg = sw.build_digits()
    box = (sdg["xs"][0], sdg["top"], sdg["xs"][3] + sdg["cell_w"], sdg["top"] + sdg["cell_h"])
    sun = sw.build_sun(slice_top_y=box[3] + 2)
    synth = wx.synth_frame("slonce", (sdg, sw.build_date_atlas(), sun, sw.build_stars(box, sun)))
    fun = HERE / "fun_options" / "out"
    return [
        ("synthwave", "Synthwave", synth),
        ("neon_rain", "Neonowy deszcz", neon),
        ("invaders", "Najeźdźcy (8-bit)", wx.invaders_frame("chmury")),
        ("fireworks", "Fajerwerki (8-bit)", crop_first(HERE / "pixel_options" / "out" / "I_fajerwerki.png")),
        ("tunnel", "Tunel prędkości", crop_first(HERE / "cyber_options" / "out" / "C_tunel.png")),
        ("portal", "Portal", gif_frame(fun / "A_portal.gif", 10)),
        ("blackhole", "Czarna dziura", gif_frame(fun / "E_czarna_dziura.gif", 15)),
        ("aurora", "Zorza polarna", gif_frame(fun / "F_zorza_neon.gif", 15)),
    ]


def grid(items, cols=4, scale=0.75, gap=10, cap=30) -> Image.Image:
    tw, th = int(W * scale), int(H * scale)
    rows = (len(items) + cols - 1) // cols
    out = Image.new("RGB", (cols * tw + (cols + 1) * gap, rows * (th + cap) + (rows + 1) * gap), (13, 10, 24))
    d = ImageDraw.Draw(out)
    f = ImageFont.truetype(str(FONTS / "Rajdhani-Bold.ttf"), 20)
    for i, (_, title, im) in enumerate(items):
        x = gap + (i % cols) * (tw + gap)
        y = gap + (i // cols) * (th + cap + gap)
        out.paste(im.resize((tw, th), Image.LANCZOS), (x, y))
        d.text((x + tw // 2, y + th + cap // 2 + 1), title, font=f, fill=(225, 205, 255), anchor="mm")
    return out


def main():
    (DOCS / "faces").mkdir(parents=True, exist_ok=True)
    (DOCS / "anim").mkdir(exist_ok=True)
    (DOCS / "weather").mkdir(exist_ok=True)
    items = tiles()
    for key, _, im in items:
        im.save(DOCS / "faces" / f"{key}.png", optimize=True)
    grid(items).save(DOCS / "faces_grid.png", optimize=True)
    fun = HERE / "fun_options" / "out"
    for src, dst in [("A_portal.gif", "portal.gif"), ("E_czarna_dziura.gif", "blackhole.gif"),
                     ("F_zorza_neon.gif", "aurora.gif")]:
        shutil.copyfile(fun / src, DOCS / "anim" / dst)
    for name in ("synthwave_pogoda.png", "neon_pogoda.png", "invaders_pogoda.png"):
        shutil.copyfile(HERE / "weather_options" / "out" / name, DOCS / "weather" / name)
    print(f"docs/img: {len(items)} tarcz, siatka, 3 animacje, 3 arkusze pogody")


if __name__ == "__main__":
    main()
