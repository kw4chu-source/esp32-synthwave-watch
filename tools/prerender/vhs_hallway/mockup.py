"""Makieta tarczy "vhs_hallway" - nagranie VHS z korytarza, postac zbliza
sie z kazda minuta. Podglad przed firmware (jak przy neon_rain).

  python tools/prerender/vhs_hallway/mockup.py
"""

from __future__ import annotations

import random
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont

HERE = Path(__file__).parent
FONTS = HERE.parent / "fonts"
OUT = HERE / "out"

W, H = 480, 320
VPX, VPY = 240, 172          # punkt zbiegu korytarza
FLOOR_K, CEIL_K, WALL_K = 148, 172, 250   # y/x krawedzi dla glebokosci z=1
Z_END = 7.0                  # glebokosc sciany na koncu korytarza


def proj_floor(z):
    return VPY + FLOOR_K / z


def proj_ceil(z):
    return VPY - CEIL_K / z


def proj_wall(z, side):
    return VPX + side * WALL_K / z


def font(px):
    return ImageFont.truetype(str(FONTS / "VT323.ttf"), px)


# ---------------------------------------------------------------- korytarz
def corridor(flicker_off=None):
    img = Image.new("RGB", (W, H), (0, 0, 0))
    d = ImageDraw.Draw(img)
    zb = Z_END
    bx0, bx1 = proj_wall(zb, -1), proj_wall(zb, 1)
    by0, by1 = proj_ceil(zb), proj_floor(zb)

    # sciany, sufit, podloga (plaskie kolory, swiatlo dochodzi nizej)
    d.polygon([(0, 0), (W, 0), (bx1, by0), (bx0, by0)], fill=(46, 52, 48))            # sufit
    d.polygon([(0, H), (W, H), (bx1, by1), (bx0, by1)], fill=(38, 40, 34))            # podloga
    d.polygon([(0, 0), (bx0, by0), (bx0, by1), (0, H)], fill=(70, 82, 74))            # lewa
    d.polygon([(W, 0), (bx1, by0), (bx1, by1), (W, H)], fill=(64, 76, 69))            # prawa
    d.rectangle([bx0, by0, bx1, by1], fill=(22, 26, 24))                              # koniec
    # drzwi na koncu, uchylone - waska smuga swiatla
    dw = (bx1 - bx0) * 0.38
    d.rectangle([VPX - dw / 2, by1 - (by1 - by0) * 0.82, VPX + dw / 2, by1], fill=(150, 165, 140))

    # plytki podlogi
    for z in np.arange(1.0, zb, 0.45):
        y = proj_floor(z)
        d.line([(proj_wall(z, -1), y), (proj_wall(z, 1), y)], fill=(30, 32, 27))
    for k in range(-6, 7):
        d.line([(VPX + k * 80, H), (VPX + k * 80 / zb, by1)], fill=(30, 32, 27))

    # drzwi na scianach bocznych
    for z0, side in ((1.6, -1), (2.9, -1), (2.2, 1), (4.2, 1)):
        z1 = z0 + 0.45
        xa, xb = proj_wall(z0, side), proj_wall(z1, side)
        ya0 = proj_floor(z0) - (proj_floor(z0) - proj_ceil(z0)) * 0.72
        yb0 = proj_floor(z1) - (proj_floor(z1) - proj_ceil(z1)) * 0.72
        d.polygon([(xa, ya0), (xb, yb0), (xb, proj_floor(z1)), (xa, proj_floor(z0))], fill=(30, 36, 33))

    # swietlowki na suficie
    lights = [2.6, 3.5, 4.6, 5.9]   # ponizej pasa cyfr
    lamp = np.zeros((H, W), np.float32)
    for i, z in enumerate(lights):
        if i == flicker_off:
            col = (40, 46, 44)
        else:
            col = (230, 255, 235)
        za, zb2 = z, z + 0.25
        pts = [(VPX - 60 / za, proj_ceil(za) + 2), (VPX + 60 / za, proj_ceil(za) + 2),
               (VPX + 60 / zb2, proj_ceil(zb2) + 2), (VPX - 60 / zb2, proj_ceil(zb2) + 2)]
        d.polygon(pts, fill=col)
        if i != flicker_off:
            # plama swiatla w dol korytarza
            yy, xx = np.mgrid[0:H, 0:W]
            cy = proj_ceil(z) + 60 / z
            lamp += np.exp(-(((xx - VPX) / (420 / z)) ** 2 + ((yy - cy) / (330 / z)) ** 2)) * 1.1

    arr = np.asarray(img, np.float32)
    # zanik swiatla w glab i oswietlenie lamp
    yy, xx = np.mgrid[0:H, 0:W]
    depth = np.clip(1 - np.hypot((xx - VPX) / 260, (yy - VPY) / 190), 0, 1)
    shade = 0.45 + 0.55 * (1 - depth) ** 1.3
    back = (abs(xx - VPX) < (bx1 - bx0) / 2) & (yy > by0) & (yy < by1)
    shade = np.where(back, 1.0, shade)
    arr = arr * shade[..., None] * (0.7 + np.clip(lamp, 0, 1.4)[..., None] * 0.75)
    return arr


# ---------------------------------------------------------------- postac
def figure(arr, z, rnd):
    feet = proj_floor(z)
    hgt = (proj_floor(z) - proj_ceil(z)) * 0.93
    s = hgt / 300.0  # skala wzgledem rysunku bazowego 300 px
    m = Image.new("L", (W, H), 0)
    d = ImageDraw.Draw(m)
    cx = VPX + 6 * s

    def P(x, y):
        return (cx + x * s, feet - y * s)

    # wydluzona, chuda sylwetka, glowa lekko przekrzywiona, dlugie rece
    d.polygon([P(-26, 0), P(-14, 150), P(-30, 230), P(-22, 250), P(22, 250), P(30, 228), P(16, 150),
               P(26, 0), P(10, 0), P(2, 120), P(-8, 0)], fill=255)
    d.ellipse([P(-17, 300)[0], P(-17, 300)[1], P(17, 252)[0], P(17, 252)[1]], fill=255)
    d.polygon([P(-28, 238), P(-48, 150), P(-44, 60), P(-38, 60), P(-36, 150), P(-20, 228)], fill=255)
    d.polygon([P(28, 238), P(46, 150), P(52, 56), P(46, 56), P(36, 150), P(20, 228)], fill=255)
    mm = np.asarray(m.filter(ImageFilter.GaussianBlur(max(0.6, 1.2 * s))), np.float32)[..., None] / 255
    arr[:] = arr * (1 - mm * 0.97) + np.array([4, 5, 5]) * mm * 0.97
    # oczy
    e = Image.new("L", (W, H), 0)
    ed = ImageDraw.Draw(e)
    for ex in (-7, 6):
        x, y = P(ex, 280)
        r = max(1.0, 2.2 * s)
        ed.ellipse([x - r, y - r * 0.6, x + r, y + r * 0.6], fill=255)
    em = np.asarray(e.filter(ImageFilter.GaussianBlur(max(0.8, 2.5 * s))), np.float32)[..., None] / 255
    arr += em * np.array([255, 245, 220]) * 1.6


# ---------------------------------------------------------------- nakladki VHS
def osd(arr, time_text, date_text, rec_on=True):
    o = Image.new("L", (W, H), 0)
    d = ImageDraw.Draw(o)
    big = font(118)
    b0 = big.getbbox("0")
    tw = big.getlength(time_text)
    d.text(((W - tw) / 2, 4 - b0[1] + 8), time_text, font=big, fill=255)
    small = font(26)
    if rec_on:
        d.ellipse([16, 14, 30, 28], fill=255)
    d.text((38, 8), "REC", font=small, fill=255)
    d.text((W - 72, 8), "SP", font=small, fill=255)
    d.polygon([(W - 34, 14), (W - 34, 30), (W - 20, 22)], fill=255)
    d.text((16, H - 34), date_text, font=small, fill=255)
    d.text((W - 118, H - 34), "CH 03", font=small, fill=255)
    om = np.asarray(o, np.float32)[..., None] / 255
    # cien pod tekstem (czytelnosc na jasnym korytarzu)
    sh = np.asarray(o.filter(ImageFilter.MaxFilter(5)).filter(ImageFilter.GaussianBlur(2)), np.float32)[..., None] / 255
    arr[:] = arr * (1 - sh * 0.75)
    color = np.array([240, 240, 235], np.float32)
    arr[:] = arr * (1 - om) + color * om
    # czerwona kropka REC
    if rec_on:
        rm = np.zeros((H, W, 1), np.float32)
        rm[14:29, 16:31] = om[14:29, 16:31]
        arr[:] = arr * (1 - rm) + np.array([235, 30, 30]) * rm


def vhs(arr, rnd, tracking_y=250):
    # rozjechane kanaly (czerwony w prawo, niebieski w lewo)
    out = arr.copy()
    out[:, 2:, 0] = arr[:, :-2, 0]
    out[:, :-1, 2] = arr[:, 1:, 2]
    # szum i ziarno
    out += np.random.default_rng(rnd.randrange(1 << 30)).normal(0, 9, out.shape)
    # pas zaklocen taśmy
    for y in range(tracking_y, min(H, tracking_y + 7)):
        out[y] = np.roll(out[y], rnd.randint(-6, 6), axis=0) * 0.8 + rnd.uniform(20, 70)
    # linie skanowania i winieta
    out[::2] *= 0.86
    yy, xx = np.mgrid[0:H, 0:W]
    out *= (1 - 0.45 * (np.hypot((xx - W / 2) / (W / 1.6), (yy - H / 2) / (H / 1.4))) ** 2)[..., None]
    # lekko chlodny, chorobliwy kolor tasmy
    out *= np.array([0.92, 1.0, 0.95])
    return out


BAYER4 = np.array([[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]])


def to565(img):
    t = np.tile(BAYER4, (H // 4 + 1, W // 4 + 1))[:H, :W][..., None] / 16.0
    steps = np.array([8, 4, 8])
    return Image.fromarray(np.clip(np.floor(np.clip(img + t * steps, 0, 255) / steps) * steps,
                                   0, 255).astype(np.uint8))


def frame(minute, flicker_off=None, rec_on=True, seed=1):
    rnd = random.Random(seed)
    arr = corridor(flicker_off)
    # minuta 0 -> koniec korytarza, minuta 59 -> tuz przed kamera
    # zbliza sie przyspieszajac: dlugo daleko, pod koniec godziny szybko
    z = 1.15 + (Z_END * 0.97 - 1.15) * (1 - minute / 59) ** 1.6
    figure(arr, z, rnd)
    osd(arr, f"23:{minute:02d}", "PIĄTEK 02.10.2026", rec_on)
    return to565(vhs(arr, rnd))


def main():
    OUT.mkdir(exist_ok=True)
    panels = [frame(5, seed=3), frame(52, flicker_off=1, rec_on=False, seed=4)]
    strip = Image.new("RGB", (W * 2 + 8, H), (0, 0, 0))
    for i, p in enumerate(panels):
        strip.paste(p, (i * (W + 8), 0))
    strip.save(OUT / "mockup.png")
    panels[0].resize((W * 2, H * 2), Image.NEAREST).save(OUT / "mockup_far_x2.png")
    panels[1].resize((W * 2, H * 2), Image.NEAREST).save(OUT / "mockup_near_x2.png")
    print("zapisano", OUT)


if __name__ == "__main__":
    main()
