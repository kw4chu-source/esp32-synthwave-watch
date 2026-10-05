"""Makiety pogody na tarczach neon_rain, synthwave, invaders (po 4 warianty).

Widzet (ikona + temperatura) w stylu tarczy + scena reagujaca na pogode.

  python tools/prerender/weather_options/mockups.py -> out/{neon,synthwave,invaders}_pogoda.png
"""

from __future__ import annotations

import math
import random
import sys
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont

HERE = Path(__file__).parent
PRE = HERE.parent
OUT = HERE / "out"
W, H = 480, 320
FONTS = PRE / "fonts"

sys.path.insert(0, str(PRE / "neon_rain"))
import gen_assets as nr  # noqa: E402

sys.modules.pop("gen_assets")
sys.path.insert(0, str(PRE / "synthwave"))
import gen_assets as sw  # noqa: E402

sys.path.insert(0, str(PRE / "pixel_options"))
import mockups as px8  # noqa: E402

DATE = "PIĄTEK  02.10.2026"


def F(name, size):
    return ImageFont.truetype(str(FONTS / name), size)


def glow_layer(img, mask_img, rgb, radius=5, k=1.0, core=1.0):
    """Swiatlo dodawane: rdzen + rozmyta poswiata (img float HxWx3)."""
    m = np.asarray(mask_img, np.float32) / 255
    g = np.asarray(mask_img.filter(ImageFilter.GaussianBlur(radius)), np.float32) / 255
    img += (m * core + g * k * 1.6)[..., None] * np.array(rgb, np.float32)


# ---------------------------------------------------------------- ikony (linie neonowe)
def icon(d, kind, cx, cy, s, width=3):
    """Ikona pogody rysowana liniami (do maski neonu). s = promien ~ polowa rozmiaru."""
    def cloud(ox, oy, k=1.0):
        r = s * 0.42 * k
        d.arc([ox - r * 2.1, oy - r * 0.9, ox - r * 0.1, oy + r * 1.1], 90, 270, width=width, fill=255)
        d.arc([ox - r * 1.3, oy - r * 1.9, ox + r * 0.7, oy + r * 0.1], 180, 330, width=width, fill=255)
        d.arc([ox - r * 0.1, oy - r * 1.4, ox + r * 1.9, oy + r * 0.6], 230, 90, width=width, fill=255)
        d.line([ox - r * 1.1, oy + r * 1.1, ox + r * 0.9, oy + r * 1.1 - 0], width=width, fill=255)
    if kind == "slonce":
        r = s * 0.45
        d.ellipse([cx - r, cy - r, cx + r, cy + r], width=width, outline=255)
        for a in range(8):
            t = a * math.pi / 4
            d.line([cx + math.cos(t) * r * 1.45, cy + math.sin(t) * r * 1.45,
                    cx + math.cos(t) * r * 1.95, cy + math.sin(t) * r * 1.95], width=width, fill=255)
    elif kind == "chmury":
        cloud(cx, cy)
    elif kind == "deszcz":
        cloud(cx, cy - s * 0.25)
        for i in range(3):
            x = cx - s * 0.45 + i * s * 0.45
            d.line([x, cy + s * 0.45, x - s * 0.15, cy + s * 0.85], width=width, fill=255)
    elif kind == "snieg":
        cloud(cx, cy - s * 0.25)
        for i in range(3):
            x, y = cx - s * 0.45 + i * s * 0.45, cy + s * 0.65
            for a in range(3):
                t = a * math.pi / 3
                d.line([x - math.cos(t) * s * 0.13, y - math.sin(t) * s * 0.13,
                        x + math.cos(t) * s * 0.13, y + math.sin(t) * s * 0.13], width=max(1, width - 1), fill=255)
    elif kind == "burza":
        cloud(cx, cy - s * 0.25)
        d.line([cx + s * 0.1, cy + s * 0.3, cx - s * 0.15, cy + s * 0.65, cx + s * 0.1, cy + s * 0.65,
                cx - s * 0.15, cy + s * 1.0], width=width, fill=255)


WEATHER = {  # nazwa: (ikona, temperatura, opis)
    "slonce": ("slonce", "18°", "SŁONECZNIE"),
    "chmury": ("chmury", "12°", "POCHMURNO"),
    "deszcz": ("deszcz", "9°", "DESZCZ"),
    "snieg": ("snieg", "-3°", "ŚNIEG"),
    "burza": ("burza", "16°", "BURZA"),
}


def label(img_pil, txt):
    d = ImageDraw.Draw(img_pil)
    d.rectangle([W - 160, H - 17, W, H], fill=(0, 0, 0))
    d.text((W - 156, H - 15), txt, font=F("Rajdhani-Bold.ttf", 14), fill=(255, 255, 255))
    return img_pil


def sheet(images, name):
    s = Image.new("RGB", (W * 2 + 8, H * 2 + 8), (40, 40, 40))
    for i, im in enumerate(images):
        s.paste(im, ((i % 2) * (W + 8), (i // 2) * (H + 8)))
    s.save(OUT / name)


# ================================================================ NEON RAIN
def neon_frame(state, assets):
    bg, dg, lut_on, date_glyphs, date_base = assets
    rnd = random.Random(7)
    img = bg.astype(np.float32).copy()
    ic, temp, desc = WEATHER[state]

    if state != "deszcz" and state != "burza":
        # bez deszczu: mgla przy ulicy
        for y in range(200, nr.STREET_Y):
            img[y] += np.array([30, 20, 45]) * ((y - 200) / (nr.STREET_Y - 200)) ** 1.5 * (0.6 if state != "snieg" else 0.3)
    if state == "burza":
        img = img * 0.8 + np.array([90, 90, 130]) * 0.35  # blysk

    # szyld z pogoda zamiast hologramu "AI-CO"
    x0, y0, x1, y1 = nr.HOLO_BOX
    img[y0:y1, x0:x1] = img[y0:y1, x0:x1] * 0.3 + np.array([2, 10, 14]) * 0.7
    m = Image.new("L", (W, H), 0)
    d = ImageDraw.Draw(m)
    d.rounded_rectangle([x0 - 2, y0 - 2, x1 + 1, y1 + 1], radius=6, outline=255, width=1)
    cx = (x0 + x1) // 2
    icon(d, ic, cx, y0 + 26, 17, width=2)
    d.text((cx, y0 + 72), temp, font=F("TiltNeon.ttf", 30), fill=255, anchor="mm")
    d.text((cx, y0 + 98), "CZĘST.", font=F("Rajdhani-Bold.ttf", 11), fill=170, anchor="mm")
    glow_layer(img, m, (0, 220, 255), radius=4, k=0.9)

    def blit(data, w, h, gx, gy, lut):
        for yy in range(h):
            for xx in range(w):
                idx = data[yy * w + xx] if data.ndim == 1 else data[yy, xx]
                if idx and 0 <= gy + yy < H and 0 <= gx + xx < W:
                    img[gy + yy, gx + xx] = np.minimum(255, img[gy + yy, gx + xx] + lut[idx])

    for i, ch in enumerate("2347"):
        blit(dg["digits"][int(ch)], dg["cell_w"], dg["cell_h"], dg["xs"][i], dg["top"], lut_on)
    blit(dg["colon"], dg["colon_w"], dg["cell_h"], dg["colon_x"], dg["top"], lut_on)
    lut_date = nr.neon_lut(nr.DATE_RGB, nr.DATE_CORE, nr.DATE_RADII)
    by_cp = {g["cp"]: g for g in date_glyphs}
    tw = sum(by_cp[ord(c)]["adv"] for c in DATE)
    x = (nr.SIGN_BOX[0] + nr.SIGN_BOX[2] - tw) // 2
    for c in DATE:
        g = by_cp[ord(c)]
        if g["w"]:
            blit(g["data"], g["w"], g["h"], x + g["xoff"], date_base + g["yoff"], lut_date)
        x += g["adv"]

    # opady
    if state in ("deszcz", "burza"):
        n = 70 if state == "deszcz" else 120
        for _ in range(n):
            xr, yr, ln = rnd.uniform(0, W + 30), rnd.uniform(-10, nr.STREET_Y), rnd.randint(9, 17)
            for k in range(ln):
                px_, py_ = int(xr - k * (0.22 if state == "deszcz" else 0.45)), int(yr) + k
                if 0 <= px_ < W and 0 <= py_ < H:
                    img[py_, px_] += (np.array([170, 195, 230]) - img[py_, px_]) * 0.55
    if state == "snieg":
        for _ in range(110):
            xs, ys, r = rnd.uniform(0, W), rnd.uniform(0, nr.STREET_Y), rnd.choice([1, 1, 2])
            img[int(ys):int(ys) + r, int(xs):int(xs) + r] = (235, 240, 255)
    if state == "burza":  # piorun nad miastem
        mb = Image.new("L", (W, H), 0)
        pts, xx, yy = [], 300, 0
        while yy < 200:
            pts.append((xx, yy))
            xx += rnd.randint(-14, 14)
            yy += rnd.randint(10, 22)
        ImageDraw.Draw(mb).line(pts, fill=255, width=2)
        glow_layer(img, mb, (200, 210, 255), radius=6, k=1.2)
    return label(Image.fromarray(nr.dither565(np.clip(img, 0, 255)).astype(np.uint8)), f"NEON: {desc}")


# ================================================================ SYNTHWAVE
def synth_frame(state, assets):
    dg, date_atlas, sun, stars = assets
    base = sw.render_preview(dg, date_atlas, sun, stars, text_date="", text_time="")
    img = np.asarray(base, np.float32).copy()
    ic, temp, desc = WEATHER[state]
    rnd = random.Random(3)

    if state in ("chmury", "deszcz", "snieg", "burza"):
        # neonowe chmury przesuwajace sie przez slonce
        dark = 0.55 if state == "chmury" else 0.8
        img[:sw.HORIZON_Y] *= (1 - 0.35 * dark)
        cl = Image.new("L", (W, H), 0)
        rim = Image.new("L", (W, H), 0)
        dc, dr = ImageDraw.Draw(cl), ImageDraw.Draw(rim)
        for (cx, cy, s) in [(170, 168, 60), (300, 150, 74), (420, 175, 50), (60, 150, 46)]:
            for (ox, oy, r) in [(-0.8, 0.15, 0.55), (-0.2, -0.25, 0.7), (0.5, 0.0, 0.6), (0.95, 0.25, 0.4)]:
                bb = [cx + ox * s - r * s, cy + oy * s - r * s * 0.7, cx + ox * s + r * s, cy + oy * s + r * s * 0.7]
                dc.ellipse(bb, fill=255)
                dr.ellipse(bb, outline=255, width=2)
        cm = np.asarray(cl, np.float32)[..., None] / 255
        rm = np.asarray(rim, np.float32) / 255 * (1 - np.asarray(cl.filter(ImageFilter.MinFilter(5)), np.float32) / 255)
        img = img * (1 - cm * 0.9) + cm * 0.9 * np.array([40, 14, 60])
        rim_img = Image.fromarray((rm * 255).astype(np.uint8))
        glow_layer(img, rim_img, (255, 70, 200), radius=4, k=0.7, core=0.9)
    if state in ("deszcz", "burza"):
        for _ in range(110):
            xr, yr, ln = rnd.uniform(0, W), rnd.uniform(0, H), rnd.randint(8, 14)
            for k in range(ln):
                px_, py_ = int(xr - k * 0.3), int(yr) + k
                if 0 <= px_ < W and 0 <= py_ < H:
                    img[py_, px_] += (np.array([120, 220, 255]) - img[py_, px_]) * 0.6
    if state == "snieg":
        for _ in range(140):
            xs, ys, r = rnd.uniform(0, W), rnd.uniform(0, H), rnd.choice([1, 2, 2])
            img[int(ys):int(ys) + r, int(xs):int(xs) + r] = (240, 235, 255)
    if state == "burza":
        mb = Image.new("L", (W, H), 0)
        pts, xx, yy = [], 120, 0
        while yy < sw.HORIZON_Y:
            pts.append((xx, yy))
            xx += rnd.randint(-12, 12)
            yy += rnd.randint(10, 20)
        ImageDraw.Draw(mb).line(pts, fill=255, width=2)
        glow_layer(img, mb, (255, 160, 255), radius=6, k=1.2)

    # cyfry nad chmurami (jak w render_preview)
    lut = sw.glyph_lut()
    def blit(g, gx, gy):
        for yy in range(g.h):
            for xx in range(g.w):
                c, a, rgb = lut[g.data[yy, xx]]
                if a:
                    p = img[gy + yy, gx + xx]
                    img[gy + yy, gx + xx] = p + (np.array(rgb) - p) * (a / 255)
    for i, ch in enumerate("2347"):
        blit(dg["digits"][int(ch)], dg["xs"][i], dg["top"])
    blit(dg["colon"], dg["colon_x"] - sw.GLOW_PAD, dg["top"])
    # data z atlasu tarczy, przesunieta w lewo
    by_cp = {g["cp"]: g for g in date_atlas}
    x = 22
    for c in DATE.replace("  ", " "):
        g = by_cp[ord(c)]
        for yy in range(g["h"]):
            for xx in range(g["w"]):
                a = g["data"][yy, xx] / 255
                if a:
                    px_, py_ = x + g["xoff"] + xx, sw.DATE_BASELINE + g["yoff"] + yy
                    img[py_, px_] += (np.array(sw.DATE_RGB) - img[py_, px_]) * a
        x += g["adv"]
    # pogoda po prawej (Orbitron, ten sam fiolet)
    f = sw.orbitron(20)
    tl = Image.new("L", (W, H), 0)
    d = ImageDraw.Draw(tl)
    d.text((W - 22, sw.DATE_BASELINE), temp + "C", font=f, fill=255, anchor="rs")
    icon(d, ic, W - 22 - d.textlength(temp + "C", font=f) - 22, sw.DATE_BASELINE - 8, 11, width=2)
    glow_layer(img, tl, np.array(sw.DATE_RGB) * 0.75, radius=3, k=0.6)
    return label(Image.fromarray(sw.dither565(np.clip(img, 0, 255))), f"SYNTHWAVE: {desc}")


# ================================================================ INVADERS
PIX_ICONS = {
    "slonce": ["..Y..Y..", "...YY...", ".YYYYYY.", "YYYYYYYY", "YYYYYYYY", ".YYYYYY.", "...YY...", "..Y..Y.."],
    "chmury": ["........", "...WW...", ".WWWWWW.", "WWWWWWWW", "WWWWWWWW", ".WWWWWW.", "........", "........"],
    "deszcz": ["...WW...", ".WWWWWW.", "WWWWWWWW", ".WWWWWW.", "........", ".L..L..L", "L..L..L.", "........"],
    "snieg": ["...WW...", ".WWWWWW.", "WWWWWWWW", ".WWWWWW.", "........", "W..W..W.", "..W..W..", "W..W..W."],
    "burza": ["...SS...", ".SSSSSS.", "SSSSSSSS", ".SSYSSS.", "...Y....", "..YY....", "...Y....", "..Y....."],
}
PIX_CLOUD = ["...SSS......", ".SSSSSSS.SS.", "SSSSSSSSSSSS", ".SSSSSSSSSS."]


def invaders_frame(state):
    rnd = random.Random(1)
    P = px8.PAL
    img = px8.canvas("K")
    ic, temp, desc = WEATHER[state]
    for _ in range(40):
        img[rnd.randrange(px8.LH), rnd.randrange(px8.LW)] = P[rnd.choice("SDW")]
    px8.text(img, 4, 2, "SCORE 2347", "W")
    # pogoda zamiast "HI 9999"
    px8.sprite(img, 112, 1, PIX_ICONS[ic])
    px8.text(img, 123, 2, temp.replace("°", "") + "C" if len(temp) > 3 else temp.replace("°", "") + "°C", "Y")
    if state in ("chmury", "deszcz", "snieg", "burza"):
        for cx, cy in [(10, 13), (70, 12), (128, 14)]:
            px8.sprite(img, cx, cy, PIX_CLOUD, "S" if state != "burza" else "D")
    for row, (spr, col) in enumerate(((px8.ALIEN_A, "M"), (px8.ALIEN_B, "C"), (px8.ALIEN_A, "G"))):
        for i in range(8):
            px8.sprite(img, 22 + i * 15, 18 + row * 9, spr[0], col)

    snow_caps = state == "snieg"

    def px(x, y, s, c, r, k):
        for yy in range(y, y + s):
            for xx in range(x, x + s):
                img[yy, xx] = P["G"]  # jednolicie jasne (dolne segmenty T byly za ciemne z daleka)
    px8.big_digits(img, "23:47", 50, 4, px)
    if snow_caps:  # snieg osiada na gornych krawedziach oslon
        for x in range(px8.LW):
            for y in range(48, 80):
                if (img[y, x] == P["G"]).all() and not (img[y - 1, x] == P["G"]).all():
                    img[y - 1, x] = P["W"]
                    break
    if state in ("deszcz", "burza"):
        for _ in range(55):
            x, y = rnd.randrange(px8.LW), rnd.randrange(12, 96)
            if (img[y, x] == P["K"]).all():
                img[y, x] = P["L"]
                if y + 1 < 97 and (img[y + 1, x] == P["K"]).all():
                    img[y + 1, x] = P["B"]
    if state == "snieg":
        for _ in range(45):
            x, y = rnd.randrange(px8.LW), rnd.randrange(12, 96)
            if (img[y, x] == P["K"]).all():
                img[y, x] = P["W"]
    if state == "burza":
        x, y = 40, 16
        while y < 46:
            img[y, x] = P["Y"]
            x += rnd.choice([-1, 0, 1])
            y += 1
    px8.sprite(img, 76, 90, px8.SHIP, "C")
    px8.rect(img, 0, 97, px8.LW, 1, "G")
    px8.text(img, 0, 99, "PIĄTEK 02.10.2026", "W", center=True)
    return label(px8.up(img), f"INVADERS: {desc}")


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    bg, _ = nr.build_background(outlines=True, hologram_on=False)
    dg = nr.build_digits()
    lut_on = nr.neon_lut(nr.TUBE_RGB, nr.TUBE_CORE, nr.DIGIT_RADII)
    date_glyphs, ascent = nr.build_date_atlas()
    nassets = (bg, dg, lut_on, date_glyphs, nr.SIGN_BOX[3] - 26 + ascent)
    sheet([neon_frame(s, nassets) for s in ("deszcz", "chmury", "burza", "snieg")], "neon_pogoda.png")
    print("neon")

    sdg = sw.build_digits()
    box = (sdg["xs"][0], sdg["top"], sdg["xs"][3] + sdg["cell_w"], sdg["top"] + sdg["cell_h"])
    sun = sw.build_sun(slice_top_y=box[3] + 2)
    sassets = (sdg, sw.build_date_atlas(), sun, sw.build_stars(box, sun))
    sheet([synth_frame(s, sassets) for s in ("slonce", "chmury", "deszcz", "snieg")], "synthwave_pogoda.png")
    print("synthwave")
    sheet([invaders_frame(s) for s in ("chmury", "deszcz", "snieg", "burza")], "invaders_pogoda.png")
    print("invaders")


if __name__ == "__main__":
    main()
