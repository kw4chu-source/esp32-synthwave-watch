"""Makiety pozostalych opcji tarczy horror: oko, cmentarz, nawiedzony las.
Kazda: 2 klatki (pokazuja glowna animacje). Tylko do wyboru - wybrana opcja
dostanie wlasny generator zasobow jak neon_rain.

  python tools/prerender/horror_options/mockups.py
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
DATE = "PIĄTEK 02.10.2026"

BAYER4 = np.array([[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]])


def font(name, px):
    return ImageFont.truetype(str(FONTS / name), px)


def to565(img):
    t = np.tile(BAYER4, (H // 4 + 1, W // 4 + 1))[:H, :W][..., None] / 16.0
    steps = np.array([8, 4, 8])
    return Image.fromarray(np.clip(np.floor(np.clip(img + t * steps, 0, 255) / steps) * steps,
                                   0, 255).astype(np.uint8))


def mask(draw_fn, blur=0):
    m = Image.new("L", (W, H), 0)
    draw_fn(ImageDraw.Draw(m))
    if blur:
        m = m.filter(ImageFilter.GaussianBlur(blur))
    return np.asarray(m, np.float32)[..., None] / 255.0


def over(arr, m, rgb, k=1.0):
    arr[:] = arr * (1 - m * k) + np.array(rgb, np.float32) * m * k


def add(arr, m, rgb, k=1.0):
    arr += m * np.array(rgb, np.float32) * k


def blobs(d, rnd, n, x0, y0, xr, yr, w, h):
    """n elips w*h rozrzuconych w prostokacie (x0..x0+xr, y0..y0+yr)."""
    for _ in range(n):
        x, y = x0 + rnd.uniform(0, xr), y0 + rnd.uniform(0, yr)
        d.ellipse([x, y, x + w, y + h], fill=255)


def text_center(d, y, txt, f, fill=255):
    b = f.getbbox(txt)
    d.text(((W - (b[2] - b[0])) / 2 - b[0], y - b[1]), txt, font=f, fill=fill)


def vignette(arr, k=0.55):
    yy, xx = np.mgrid[0:H, 0:W]
    arr *= (1 - k * (np.hypot((xx - W / 2) / (W / 1.5), (yy - H / 2) / (H / 1.3))) ** 2)[..., None]


# ================================================================ OKO
def eye(look=(0.55, -0.1), lid=0.0, pupil=1.0, seed=1):
    rnd = random.Random(seed)
    arr = np.zeros((H, W, 3), np.float32)
    yy, xx = np.mgrid[0:H, 0:W]
    arr[:] = np.array([28, 2, 6]) * (1 - np.hypot((xx - 240) / 330, (yy - 110) / 230))[..., None].clip(0, 1) + 4

    # zylki na tle (rozgalezione krzywe)
    def veins(d):
        for _ in range(26):
            a = rnd.uniform(0, 2 * math.pi)
            x, y = 240 + math.cos(a) * 165, 108 + math.sin(a) * 85
            for _ in range(rnd.randint(6, 12)):
                a += rnd.uniform(-0.6, 0.6)
                nx, ny = x + math.cos(a) * 14, y + math.sin(a) * 14
                d.line([(x, y), (nx, ny)], fill=255, width=rnd.choice((1, 1, 2)))
                x, y = nx, ny
    add(arr, mask(veins, 0.6), (120, 8, 14), 0.9)

    cx, cy, ew, eh = 240, 108, 168, 78
    # bialko z czerwonymi naczynkami
    almond = mask(lambda d: d.chord([cx - ew, cy - eh * 1.6, cx + ew, cy + eh * 1.6], 200, 340, fill=255) or
                  d.chord([cx - ew, cy - eh * 1.6 - 1, cx + ew, cy + eh * 1.6], 20, 160, fill=255))
    almond = mask(lambda d: d.polygon([(cx - ew + ew * 2 * t, cy - eh * math.sin(math.pi * t)) for t in np.linspace(0, 1, 60)] +
                                      [(cx + ew - ew * 2 * t, cy + eh * math.sin(math.pi * t)) for t in np.linspace(0, 1, 60)],
                                      fill=255), 1)
    sclera = np.array([225, 210, 195], np.float32) * (1 - 0.35 * (np.hypot((xx - cx) / ew, (yy - cy) / eh) ** 2)[..., None].clip(0, 1))
    arr[:] = arr * (1 - almond) + sclera * almond

    def capillaries(d):
        for _ in range(22):
            side = rnd.choice((-1, 1))
            x, y = cx + side * rnd.uniform(ew * 0.65, ew * 0.95), cy + rnd.uniform(-eh * 0.4, eh * 0.4)
            a = math.pi if side > 0 else 0
            for _ in range(rnd.randint(4, 8)):
                a += rnd.uniform(-0.7, 0.7)
                nx, ny = x + math.cos(a) * 9, y + math.sin(a) * 9
                d.line([(x, y), (nx, ny)], fill=255)
                x, y = nx, ny
    over(arr, mask(capillaries, 0.4) * almond, (190, 20, 30), 0.8)

    # teczowka + zrenica (przesuniecie = gdzie patrzy)
    ix, iy = cx + look[0] * ew * 0.5, cy + look[1] * eh * 0.5
    ir = 50
    iris = mask(lambda d: d.ellipse([ix - ir, iy - ir, ix + ir, iy + ir], fill=255), 0.8) * almond
    ang = np.arctan2(yy - iy, xx - ix)
    rad = np.hypot(xx - ix, yy - iy) / ir
    tex = 0.6 + 0.4 * np.sin(ang * 23 + rad * 6) ** 2
    iris_col = (np.array([170, 120, 20]) * (1 - rad) + np.array([90, 30, 10]) * rad)[..., :] if False else None
    col = np.stack([200 - 120 * rad, 150 - 110 * rad, 30 + 0 * rad], -1).clip(0, 255) * tex[..., None]
    arr[:] = arr * (1 - iris) + col * iris
    pr = 20 * pupil
    over(arr, mask(lambda d: d.ellipse([ix - pr * 0.7, iy - pr, ix + pr * 0.7, iy + pr], fill=255), 0.8) * almond, (2, 0, 0))
    add(arr, mask(lambda d: d.ellipse([ix - 22, iy - 26, ix - 12, iy - 16], fill=255), 1.2) * almond, (255, 255, 255), 0.85)

    # powieki (lid 0 = otwarte, 1 = zamkniete)
    if lid > 0:
        top = mask(lambda d: d.polygon([(cx - ew - 4, cy)] + [(cx - ew + ew * 2 * t, cy - eh * math.sin(math.pi * t) * (1 - 2 * lid) - 4)
                                                            for t in np.linspace(0, 1, 60)] + [(cx + ew + 4, cy), (cx + ew + 4, 0), (cx - ew - 4, 0)], fill=255), 1)
        over(arr, top * almond, (70, 22, 22))
    # obwodka powiek + rzesy
    rim = mask(lambda d: d.line([(cx - ew + ew * 2 * t, cy - eh * math.sin(math.pi * t) * (1 - 2 * lid)) for t in np.linspace(0, 1, 60)], fill=255, width=3), 0.8)
    over(arr, rim, (25, 5, 5))

    def lashes(d):
        for t in np.linspace(0.08, 0.92, 19):
            x, y = cx - ew + ew * 2 * t, cy - eh * math.sin(math.pi * t) * (1 - 2 * lid)
            a = -math.pi / 2 + (t - 0.5) * 1.9
            d.line([(x, y), (x + math.cos(a) * 16, y + math.sin(a) * 16)], fill=255, width=2)
    over(arr, mask(lashes, 0.5), (10, 2, 2))

    # godzina: czerwone, cieknace cyfry + data
    t = mask(lambda d: text_center(d, 212, "23:47", font("Creepster.ttf", 96)), 0)
    add(arr, mask(lambda d: text_center(d, 212, "23:47", font("Creepster.ttf", 96)), 6), (200, 0, 10), 0.8)
    over(arr, t, (235, 25, 30))
    over(arr, mask(lambda d: text_center(d, 294, DATE, font("SpecialElite.ttf", 20)), 0), (200, 170, 160))
    vignette(arr, 0.5)
    return to565(arr)


# ================================================================ CMENTARZ
def cemetery(ghost_x=78, raven_eye=True, seed=2):
    rnd = random.Random(seed)
    arr = np.zeros((H, W, 3), np.float32)
    for y in range(H):
        t = y / H
        arr[y] = np.array([6, 10, 26]) * (1 - t) + np.array([24, 34, 52]) * t
    # ksiezyc z poswiata
    add(arr, mask(lambda d: d.ellipse([370, 18, 438, 86], fill=255), 22), (120, 140, 170), 0.9)
    over(arr, mask(lambda d: d.ellipse([376, 24, 432, 80], fill=255), 1), (225, 228, 210))
    over(arr, mask(lambda d: blobs(d, rnd, 6, 386, 34, 32, 34, 9, 8), 1.5), (180, 182, 170), 0.6)
    # chmury przed ksiezycem
    add(arr, mask(lambda d: [d.ellipse([300 + i * 30, 60 + rnd.randint(-6, 6), 380 + i * 30, 78 + rnd.randint(-6, 6)], fill=255) for i in range(5)], 6), (30, 36, 52), 0.8)

    # martwe drzewo po prawej
    def branch(d, x, y, a, ln, wd, depth):
        if depth == 0 or ln < 4:
            return
        nx, ny = x + math.cos(a) * ln, y + math.sin(a) * ln
        d.line([(x, y), (nx, ny)], fill=255, width=max(1, int(wd)))
        for _ in range(2 if depth > 2 else 3):
            branch(d, nx, ny, a + rnd.uniform(-0.75, 0.75), ln * rnd.uniform(0.6, 0.78), wd * 0.65, depth - 1)
    over(arr, mask(lambda d: branch(d, 448, 300, -math.pi / 2 - 0.15, 70, 11, 7)), (4, 5, 9))
    # ogrodzenie
    def fence(d):
        d.line([(0, 228), (W, 222)], fill=255, width=2)
        d.line([(0, 252), (W, 246)], fill=255, width=2)
        for x in range(4, W, 14):
            d.line([(x, 268), (x, 214)], fill=255, width=2)
            d.polygon([(x - 3, 216), (x, 208), (x + 3, 216)], fill=255)
    over(arr, mask(fence), (8, 10, 16))
    # male nagrobki w tle
    for x, w_, h_ in ((20, 34, 46), (420, 30, 40), (60, 26, 30)):
        over(arr, mask(lambda d: d.rounded_rectangle([x, 300 - h_, x + w_, 300], radius=10, fill=255)), (40, 44, 52))
    # wielki nagrobek z wyryta godzina
    stone = mask(lambda d: (d.rounded_rectangle([88, 70, 392, 300], radius=4, fill=255),
                            d.pieslice([88, 18, 392, 170], 180, 360, fill=255)), 0.8)
    yy, xx = np.mgrid[0:H, 0:W]
    noise = np.random.default_rng(seed).normal(0, 7, (H, W, 1))
    stone_col = np.array([92, 96, 104], np.float32) * (1.05 - 0.3 * ((yy - 40) / 260))[..., None] + noise
    arr[:] = arr * (1 - stone) + stone_col * stone
    # mech i pekniecie
    over(arr, mask(lambda d: d.line([(330, 60), (318, 92), (326, 118), (312, 150)], fill=255, width=2), 0.5) * stone, (40, 42, 46))
    add(arr, mask(lambda d: blobs(d, rnd, 30, 90, 250, 290, 45, 14, 12), 3) * stone, (-30, -10, -40), 0.6)
    # wyryte litery: ciemne wnetrze + jasna krawedz od dolu
    f = font("Cinzel.ttf", 94)
    deep = mask(lambda d: text_center(d, 96, "23:47", f))
    light = mask(lambda d: text_center(d, 98, "23:47", f))
    over(arr, np.clip(light - deep, 0, 1), (150, 154, 160))
    over(arr, deep, (26, 26, 30))
    fs = font("Cinzel.ttf", 19)
    over(arr, mask(lambda d: text_center(d, 214, DATE, fs)), (32, 32, 36))
    over(arr, mask(lambda d: text_center(d, 244, "†  R.I.P.  †", font("Cinzel.ttf", 17))), (40, 40, 44))
    # kruk na nagrobku
    over(arr, mask(lambda d: (d.ellipse([214, 4, 252, 30], fill=255), d.ellipse([244, 0, 262, 14], fill=255),
                              d.polygon([(260, 6), (272, 9), (260, 11)], fill=255),
                              d.polygon([(214, 18), (196, 30), (220, 26)], fill=255)), 0.6), (6, 6, 10))
    if raven_eye:
        add(arr, mask(lambda d: d.ellipse([253, 4, 257, 8], fill=255), 1.5), (255, 30, 20), 1.6)
    # mgla przy ziemi
    for i in range(3):
        add(arr, mask(lambda d: blobs(d, rnd, 9, -60, 256, W, 26, 150, 40), 12), (90, 100, 115), 0.35)
    # duch
    gx = ghost_x
    g = mask(lambda d: (d.ellipse([gx - 22, 120, gx + 22, 166], fill=255),
                        d.polygon([(gx - 22, 145), (gx + 22, 145), (gx + 28, 230), (gx + 14, 218), (gx + 4, 236),
                                   (gx - 8, 220), (gx - 20, 238), (gx - 30, 226)], fill=255)), 4)
    add(arr, g, (170, 200, 220), 0.55)
    over(arr, mask(lambda d: (d.ellipse([gx - 12, 136, gx - 4, 148], fill=255), d.ellipse([gx + 4, 136, gx + 12, 148], fill=255),
                              d.ellipse([gx - 5, 152, gx + 5, 166], fill=255)), 1), (10, 14, 20), 0.85)
    vignette(arr, 0.6)
    return to565(arr)


# ================================================================ LAS
def forest(eyes=((60, 210, (255, 210, 40)), (418, 196, (255, 40, 20)), (120, 238, (180, 255, 60))), bats_x=0, seed=3):
    rnd = random.Random(seed)
    arr = np.zeros((H, W, 3), np.float32)
    for y in range(H):
        t = y / H
        arr[y] = np.array([10, 4, 22]) * (1 - t) + np.array([30, 18, 34]) * t
    # wielki ksiezyc za cyframi
    add(arr, mask(lambda d: d.ellipse([140, 18, 340, 218], fill=255), 30), (120, 110, 90), 0.7)
    moon = mask(lambda d: d.ellipse([150, 28, 330, 208], fill=255), 1)
    yy, xx = np.mgrid[0:H, 0:W]
    mc = np.array([235, 225, 190], np.float32) * (1 - 0.25 * np.hypot((xx - 240) / 90, (yy - 118) / 90))[..., None].clip(0, 1)
    arr[:] = arr * (1 - moon) + mc * moon
    over(arr, mask(lambda d: blobs(d, rnd, 9, 170, 50, 130, 130, 20, 16), 3) * moon, (190, 180, 150), 0.6)

    def tree(d, x, base, h, lean):
        d.polygon([(x - 9, base), (x + 9, base), (x + 4 + lean, base - h), (x - 2 + lean, base - h)], fill=255)

        def br(px, py, a, ln, wd, depth):
            if depth == 0:
                return
            nx, ny = px + math.cos(a) * ln, py + math.sin(a) * ln
            d.line([(px, py), (nx, ny)], fill=255, width=max(1, int(wd)))
            for _ in range(2):
                br(nx, ny, a + rnd.uniform(-0.8, 0.8), ln * rnd.uniform(0.6, 0.8), wd * 0.62, depth - 1)
        for k in range(5):
            br(x + lean * (0.5 + k * 0.1), base - h * (0.45 + k * 0.11), -math.pi / 2 + rnd.uniform(-1.3, 1.3), h * 0.28, 6, 5)
    # drzewa - dalsze jasniejsze (mgla), blizsze czarne
    far = mask(lambda d: [tree(d, x, 300, rnd.randint(150, 210), rnd.randint(-15, 15)) for x in (40, 110, 380, 450)], 1.2)
    over(arr, far, (22, 18, 30))
    add(arr, mask(lambda d: blobs(d, rnd, 8, -100, 200, W, 50, 200, 90), 18), (60, 50, 70), 0.45)
    near = mask(lambda d: [tree(d, x, 330, rnd.randint(260, 320), rnd.randint(-25, 25)) for x in (6, 76, 404, 472)])
    over(arr, near, (3, 2, 6))
    over(arr, mask(lambda d: d.rectangle([0, 292, W, H], fill=255), 2), (6, 4, 10))
    # oczy w ciemnosci
    for x, y, rgb in eyes:
        e = mask(lambda d: (d.ellipse([x - 7, y - 2, x - 2, y + 2], fill=255), d.ellipse([x + 2, y - 2, x + 7, y + 2], fill=255)), 0.6)
        add(arr, mask(lambda d: (d.ellipse([x - 7, y - 2, x - 2, y + 2], fill=255), d.ellipse([x + 2, y - 2, x + 7, y + 2], fill=255)), 3), rgb, 1.3)
        add(arr, e, rgb, 1.2)
    # nietoperze na tle ksiezyca
    def bat(d, x, y, s):
        d.polygon([(x, y), (x - 6 * s, y - 4 * s), (x - 12 * s, y - 1 * s), (x - 9 * s, y + 2 * s), (x - 4 * s, y + 1 * s),
                   (x, y + 3 * s), (x + 4 * s, y + 1 * s), (x + 9 * s, y + 2 * s), (x + 12 * s, y - 1 * s), (x + 6 * s, y - 4 * s)], fill=255)
    over(arr, mask(lambda d: [bat(d, x + bats_x, y, s) for x, y, s in ((200, 70, 1.6), (262, 52, 1.2), (300, 92, 1.0))]), (8, 4, 12))
    # godzina: kosciana biel z zielonkawa poswiata na tle ksiezyca
    f = font("Creepster.ttf", 104)
    t = mask(lambda d: text_center(d, 70, "23:47", f))
    # czarne cyfry-sylwetki z zielona poswiata - czytelne na jasnym ksiezycu
    add(arr, mask(lambda d: text_center(d, 70, "23:47", f), 6), (40, 200, 70), 0.9)
    over(arr, t, (6, 8, 6))
    over(arr, mask(lambda d: text_center(d, 296, DATE, font("SpecialElite.ttf", 18)), 1), (180, 190, 160))
    vignette(arr, 0.5)
    return to565(arr)


def strip(frames):
    s = Image.new("RGB", (W * len(frames) + 8 * (len(frames) - 1), H), (0, 0, 0))
    for i, f in enumerate(frames):
        s.paste(f, (i * (W + 8), 0))
    return s


def main():
    OUT.mkdir(exist_ok=True)
    strip([eye(), eye(look=(-0.5, 0.2), lid=0.35, pupil=0.55, seed=1)]).save(OUT / "eye.png")
    strip([cemetery(), cemetery(ghost_x=150, raven_eye=False)]).save(OUT / "cemetery.png")
    strip([forest(), forest(eyes=((60, 210, (255, 210, 40)), (300, 250, (255, 40, 20))), bats_x=-60)]).save(OUT / "forest.png")
    print("zapisano", OUT)


if __name__ == "__main__":
    main()
