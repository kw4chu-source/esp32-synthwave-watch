"""Makiety (animowane GIF) tarcz "jasne kreskowki na ciemnym tle":
  A. Portal   - zielony wirujacy portal (klimat Rick i Morty), wylatuja z niego rzeczy, krazy spodek
  B. Glebiny  - swiecace meduzy, lawica rybek, babelki, plankton
  C. Lampa    - lampa lava: kolorowe kule plynna sie lacza i rozdzielaja za cyframi
  D. Orbity   - kreskowkowy kosmos: planety krazace wokol godziny, kometa, rakieta

  python tools/prerender/fun_options/mockups.py  -> out/*.gif + out/sheet.png
"""

from __future__ import annotations

import math
import random
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont

HERE = Path(__file__).parent
FONTS = HERE.parent / "fonts"
OUT = HERE / "out"
W, H = 480, 320
N = 30  # klatek w petli
TIME, DATE = "21:47", "SOBOTA  04.10.2026"


def F(name, px):
    return ImageFont.truetype(str(FONTS / name), px)


def glow(layer: Image.Image, r: float, k: float = 1.4) -> np.ndarray:
    a = np.asarray(layer, np.float32)
    return a + np.asarray(layer.filter(ImageFilter.GaussianBlur(r)), np.float32) * k


def finish(base: np.ndarray) -> Image.Image:
    return Image.fromarray(np.clip(base, 0, 255).astype(np.uint8))


def big_text(d: ImageDraw.ImageDraw, xy, s, font, fill, stroke, sw, anchor="mm"):
    d.text(xy, s, font=font, fill=fill, anchor=anchor, stroke_width=sw, stroke_fill=stroke)


def stars(rng, n):
    return [(rng.randrange(W), rng.randrange(H), rng.random()) for _ in range(n)]


# ---------------------------------------------------------------- A. Portal
def portal(i):
    t = i / N
    rng = random.Random(1)
    img = Image.new("RGB", (W, H), (4, 6, 12))
    d = ImageDraw.Draw(img)
    for x, y, p in stars(rng, 70):
        b = int(90 + 120 * (0.5 + 0.5 * math.sin(2 * math.pi * (t * 2 + p))))
        d.point((x, y), (b, b, b))
    light = Image.new("RGB", (W, H), 0)
    ld = ImageDraw.Draw(light)
    cx, cy, rx, ry = 240, 140, 150, 112
    # tarcza portalu: koncentryczne elipsy od ciemnej zieleni do jaskrawej
    for k in range(24, 0, -1):
        f = k / 24
        c = (int(30 + 150 * (1 - f)), int(140 + 115 * (1 - f) ** 0.5), int(20 + 40 * (1 - f)))
        ld.ellipse([cx - rx * f, cy - ry * f, cx + rx * f, cy + ry * f], fill=c)
    # wirujace ramiona spirali
    for arm in range(5):
        pts = []
        for s in range(60):
            u = s / 59
            a = 2 * math.pi * (arm / 5 + t) + u * 5.0
            pts.append((cx + math.cos(a) * rx * u, cy + math.sin(a) * ry * u))
        ld.line(pts, fill=(200, 255, 120), width=4)
    # postrzepiona krawedz
    edge = []
    for s in range(120):
        a = 2 * math.pi * s / 120
        w = 1 + 0.05 * math.sin(a * 9 + t * 2 * math.pi * 2) + 0.03 * math.sin(a * 23 - t * 2 * math.pi * 3)
        edge.append((cx + math.cos(a) * rx * w, cy + math.sin(a) * ry * w))
    ld.line(edge + edge[:1], fill=(230, 255, 160), width=5)
    base = np.asarray(img, np.float32) + glow(light, 10, 0.9)
    img = finish(base)
    d = ImageDraw.Draw(img)
    # rzeczy wylatujace z portalu: rosna i odlatuja
    things = [("cube", 0.0, 0.3), ("eye", 0.33, 2.2), ("cube", 0.66, 4.0), ("eye", 0.5, 5.3)]
    for kind, off, ang in things:
        u = (t + off) % 1
        r = 20 + u * 260
        x, y = cx + math.cos(ang) * r, cy + math.sin(ang) * r * 0.75
        s = 4 + u * 22
        if kind == "cube":
            rot = u * 6
            pts = [(x + math.cos(rot + q * math.pi / 2) * s, y + math.sin(rot + q * math.pi / 2) * s) for q in range(4)]
            d.polygon(pts, fill=(255, 120, 200), outline=(255, 255, 255), width=2)
        else:
            d.ellipse([x - s, y - s, x + s, y + s], fill=(250, 250, 240), outline=(40, 40, 40), width=2)
            d.ellipse([x - s * 0.45, y - s * 0.45, x + s * 0.45, y + s * 0.45], fill=(40, 140, 255))
            d.ellipse([x - s * 0.18, y - s * 0.18, x + s * 0.18, y + s * 0.18], fill=(0, 0, 0))
    # spodek krazacy wokol portalu
    a = 2 * math.pi * t
    sx, sy = cx + math.cos(a) * 205, cy + 20 + math.sin(a) * 110
    d.ellipse([sx - 26, sy - 6, sx + 26, sy + 8], fill=(180, 190, 200), outline=(40, 40, 50), width=2)
    d.ellipse([sx - 11, sy - 15, sx + 11, sy + 2], fill=(120, 230, 255), outline=(40, 40, 50), width=2)
    for q in range(3):
        on = (i // 3 + q) % 3 == 0
        d.ellipse([sx - 18 + q * 15, sy + 1, sx - 12 + q * 15, sy + 6], fill=(255, 240, 60) if on else (90, 90, 60))
    big_text(d, (240, 140), TIME, F("Creepster.ttf", 118), (190, 255, 60), (10, 40, 0), 6)
    big_text(d, (240, 290), DATE, F("Audiowide-Regular.ttf", 20), (190, 255, 60), (0, 0, 0), 3)
    return img


# ---------------------------------------------------------------- B. Glebiny
def deep(i):
    t = i / N
    rng = random.Random(2)
    y_ = np.linspace(0, 1, H)[:, None, None]
    base = np.zeros((H, W, 3), np.float32) + np.array([2, 8, 22]) * (1 - y_) + np.array([0, 2, 8]) * y_
    light = Image.new("RGB", (W, H), 0)
    d = ImageDraw.Draw(light)
    # plankton
    for x, y, p in stars(rng, 90):
        b = 0.5 + 0.5 * math.sin(2 * math.pi * (t + p))
        d.point((x, (y - t * 20) % H), (int(40 * b), int(200 * b), int(180 * b)))
    # meduzy: pulsujacy kapelusz + falujace macki
    jellies = [(90, 120, (255, 80, 200), 0.0, 46), (395, 95, (90, 200, 255), 0.4, 40), (330, 250, (190, 120, 255), 0.7, 30)]
    for jx, jy, col, ph, R in jellies:
        pulse = math.sin(2 * math.pi * (t + ph))
        jy2 = jy - 10 * pulse
        rw, rh = R * (1 + 0.12 * pulse), R * (0.75 - 0.1 * pulse)
        d.chord([jx - rw, jy2 - rh, jx + rw, jy2 + rh], 180, 360, fill=tuple(int(c * 0.55) for c in col), outline=col, width=3)
        for k in range(6):
            x0 = jx - rw * 0.8 + k * rw * 1.6 / 5
            pts = [(x0 + 6 * math.sin(2 * math.pi * (t * 2 + ph) + s * 0.35 + k), jy2 + s * 6) for s in range(14)]
            d.line(pts, fill=col, width=2)
        d.ellipse([jx - 6, jy2 - rh * 0.6, jx + 6, jy2 - rh * 0.6 + 8], fill=(255, 255, 255))
    # lawica rybek
    for k in range(12):
        fx = (W + 40) - ((t + k * 0.037) % 1) * (W + 80) - (k % 4) * 14
        fy = 225 + (k // 4) * 12 + 5 * math.sin(2 * math.pi * t * 2 + k)
        d.polygon([(fx, fy), (fx + 12, fy - 5), (fx + 12, fy + 5)], fill=(255, 220, 80))
        d.polygon([(fx + 12, fy), (fx + 18, fy - 5), (fx + 18, fy + 5)], fill=(255, 150, 40))
    # babelki
    for k in range(14):
        bx = 30 + (k * 37) % 420 + 4 * math.sin(2 * math.pi * t * 2 + k)
        by = H - ((t + k / 14) % 1) * (H + 20)
        r = 2 + k % 4
        d.ellipse([bx - r, by - r, bx + r, by + r], outline=(150, 230, 255), width=1)
    base += glow(light, 6, 1.2)
    img = finish(base)
    d = ImageDraw.Draw(img)
    tl = Image.new("RGB", (W, H), 0)
    big_text(ImageDraw.Draw(tl), (240, 145), TIME, F("TiltNeon.ttf", 120), (120, 255, 240), (0, 0, 0), 0)
    img = finish(np.asarray(img, np.float32) + glow(tl, 8, 1.3))
    d = ImageDraw.Draw(img)
    big_text(d, (240, 295), DATE, F("TiltNeon.ttf", 22), (150, 230, 255), (0, 0, 0), 0)
    return img


# ---------------------------------------------------------------- C. Lampa lava
def lava(i):
    t = i / N
    s = 4  # liczone w 120x80
    ys, xs = np.mgrid[0:H // s, 0:W // s].astype(np.float32)
    field = np.zeros_like(xs)
    hue = np.zeros((H // s, W // s, 3), np.float32)
    blobs = [((255, 40, 160), 0.0, 1, 14), ((255, 140, 0), 0.25, -1, 12), ((255, 230, 40), 0.5, 1, 10),
             ((120, 60, 255), 0.75, -1, 13), ((0, 220, 200), 0.12, 1, 11), ((255, 70, 70), 0.62, -1, 9)]
    for k, (col, ph, dirn, r) in enumerate(blobs):
        a = 2 * math.pi * (t * dirn + ph)
        bx = 60 + 45 * math.sin(a + k) * (0.6 + 0.4 * math.cos(k))
        by = 40 + 30 * math.sin(2 * a + k * 1.7)
        f = r * r / ((xs - bx) ** 2 + (ys - by) ** 2 + 1)
        field += f
        hue += f[..., None] * np.array(col, np.float32)
    hue /= field[..., None] + 1e-6
    inside = np.clip((field - 1.0) * 3, 0, 1)
    rim = np.clip(1 - np.abs(field - 1.0) * 6, 0, 1)
    rgb = hue * (inside[..., None] * 0.9) + 255 * rim[..., None] * 0.6
    small = Image.fromarray(np.clip(rgb, 0, 255).astype(np.uint8))
    layer = small.resize((W, H), Image.BICUBIC)
    base = np.zeros((H, W, 3), np.float32) + np.array([8, 2, 14])
    base += glow(layer, 8, 0.6)
    img = finish(base)
    d = ImageDraw.Draw(img)
    big_text(d, (240, 145), TIME, F("Audiowide-Regular.ttf", 110), (255, 255, 255), (20, 0, 30), 7)
    big_text(d, (240, 290), DATE, F("Audiowide-Regular.ttf", 20), (255, 255, 255), (20, 0, 30), 4)
    return img


# ---------------------------------------------------------------- D. Orbity
def orbits(i):
    t = i / N
    rng = random.Random(4)
    img = Image.new("RGB", (W, H), (6, 4, 18))
    d = ImageDraw.Draw(img)
    for x, y, p in stars(rng, 90):
        b = int(80 + 150 * (0.5 + 0.5 * math.sin(2 * math.pi * (t * 3 + p))))
        d.point((x, y), (b, b, int(b * 0.9)))
    light = Image.new("RGB", (W, H), 0)
    ld = ImageDraw.Draw(light)
    cx, cy = 240, 145
    # kometa przez caly ekran
    u = t
    hx, hy = -60 + u * (W + 120), 30 + u * 70
    for k in range(30):
        f = 1 - k / 30
        ld.ellipse([hx - k * 6 - 3 * f, hy - k * 1.6 - 3 * f, hx - k * 6 + 3 * f, hy - k * 1.6 + 3 * f],
                   fill=(int(120 * f), int(220 * f), int(255 * f)))
    ld.ellipse([hx - 5, hy - 5, hx + 5, hy + 5], fill=(255, 255, 255))
    base = np.asarray(img, np.float32) + glow(light, 5, 1.0)
    img = finish(base)
    d = ImageDraw.Draw(img)
    planets = [(205, 120, 18, (255, 120, 60), 1, 0.0, False), (175, 100, 13, (80, 200, 255), -1, 0.3, False),
               (230, 135, 22, (255, 210, 80), 1, 0.6, True), (150, 82, 9, (120, 255, 140), 1, 0.85, False)]
    # najpierw te z tylu (gorna polowa orbity), potem cyfry, potem z przodu
    drawn = []
    for rx, ry, r, col, dirn, ph, ring in planets:
        a = 2 * math.pi * (t * dirn + ph)
        drawn.append((math.sin(a), cx + math.cos(a) * rx, cy + math.sin(a) * ry, r, col, ring))
        d.ellipse([cx - rx, cy - ry, cx + rx, cy + ry], outline=(50, 40, 90), width=1)

    def planet(x, y, r, col, ring):
        if ring:
            d.ellipse([x - r * 2, y - r * 0.5, x + r * 2, y + r * 0.5], outline=(255, 240, 180), width=3)
        d.ellipse([x - r, y - r, x + r, y + r], fill=col, outline=(20, 10, 30), width=3)
        d.ellipse([x - r * 0.6, y - r * 0.6, x - r * 0.1, y - r * 0.1], fill=tuple(min(255, c + 70) for c in col))

    for z, x, y, r, col, ring in drawn:
        if z < 0:
            planet(x, y, r, col, ring)
    big_text(d, (cx, cy), TIME, F("Orbitron.ttf", 96), (255, 250, 220), (40, 20, 80), 6)
    for z, x, y, r, col, ring in drawn:
        if z >= 0:
            planet(x, y, r, col, ring)
    # rakieta po osemce na dole
    a = 2 * math.pi * t
    rx_, ry_ = 240 + 200 * math.sin(a), 268 + 22 * math.sin(2 * a)
    ang = math.atan2(44 * math.cos(2 * a), 200 * math.cos(a))
    def rot(px_, py_):
        return (rx_ + px_ * math.cos(ang) - py_ * math.sin(ang), ry_ + px_ * math.sin(ang) + py_ * math.cos(ang))
    d.polygon([rot(-14, -7), rot(10, -7), rot(20, 0), rot(10, 7), rot(-14, 7)], fill=(240, 240, 250), outline=(30, 20, 40))
    d.polygon([rot(-14, -7), rot(-22, -13), rot(-8, -7)], fill=(255, 60, 90))
    d.polygon([rot(-14, 7), rot(-22, 13), rot(-8, 7)], fill=(255, 60, 90))
    fl = 10 + 6 * ((i % 3) / 2)
    d.polygon([rot(-14, -4), rot(-14 - fl, 0), rot(-14, 4)], fill=(255, 200, 40))
    d.ellipse([*rot(2, 0)][:1] and [rot(2, 0)[0] - 3, rot(2, 0)[1] - 3, rot(2, 0)[0] + 3, rot(2, 0)[1] + 3], fill=(80, 200, 255))
    big_text(d, (240, 305), DATE, F("Orbitron.ttf", 15), (200, 190, 255), (0, 0, 0), 2)
    return img


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    opts = [("A_portal", portal), ("B_glebiny", deep), ("C_lampa", lava), ("D_orbity", orbits)]
    sheet = Image.new("RGB", (W * 2 + 10, H * 2 + 10), (40, 40, 40))
    for n, (name, fn) in enumerate(opts):
        frames = [fn(i) for i in range(N)]
        frames[0].save(OUT / f"{name}.gif", save_all=True, append_images=frames[1:], duration=90, loop=0)
        sheet.paste(frames[N // 3], ((n % 2) * (W + 10), (n // 2) * (H + 10)))
        print(name)
    sheet.save(OUT / "sheet.png")


if __name__ == "__main__":
    main()
