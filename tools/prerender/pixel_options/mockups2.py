"""Druga seria tarcz 8-bit (ta sama paleta i rozdzielczosc co mockups.py):
  G. Cegielki   - cyfry z cegiel, pilka je rozbija, przy zmianie minuty mur sie odbudowuje
  H. Wyscig     - pseudo-3D droga, pasy i pobocze pedza, samochod zmienia pas
  I. Fajerwerki - nocne miasto, wybuchy fajerwerkow, zapalajace sie okna

  python tools/prerender/pixel_options/mockups2.py
"""

from __future__ import annotations

import math
import random

import numpy as np

from mockups import (LH, LW, OUT, PAL, DATE, big_digits, block, canvas, rect, sprite, strip, text, up)


# ================================================================ G. CEGIELKI
PADDLE_COLORS = ["M", "C", "Y", "G", "O", "R", "B"]


def bricks(frame=0, seed=4):
    rnd = random.Random(seed)
    img = canvas("K")
    rect(img, 0, 0, LW, 1, "S")
    rect(img, 0, 0, 1, LH, "S")
    rect(img, LW - 1, 0, 1, LH, "S")
    text(img, 4, 3, "1UP 2347", "W")
    text(img, 108, 3, "x3", "R")
    broken = {(rnd.randrange(30), rnd.randrange(9)) for _ in range(9)} if frame else set()

    def px(x, y, s, c, r, k):
        if (x // 4, r) in broken:
            return
        col = PADDLE_COLORS[r]
        block(img, x, y, s, col)
    big_digits(img, "23:47", 20, 4, px)
    # pilka i jej slad
    bx, by = (70, 66) if frame == 0 else (92, 50)
    for i, (dx, dy) in enumerate(((-6, 6), (-4, 4), (-2, 2))):
        img[by + dy, bx + dx] = PAL["D"] if i < 2 else PAL["S"]
    rect(img, bx, by, 2, 2, "W")
    if frame:  # odlamki rozbitej cegly
        for dx, dy in ((-3, -2), (2, -3), (4, 1), (-2, 3)):
            img[44 + dy, 96 + dx] = PAL["Y"]
    # paletka
    px_ = 58 if frame == 0 else 80
    rect(img, px_, 88, 22, 3, "C")
    rect(img, px_ + 1, 88, 20, 1, "W")
    text(img, 0, 98, DATE, "W", center=True)
    return up(img)


# ================================================================ H. WYSCIG
CAR = ["....RRRRRR....", "...RWWWWWWR...", "..RRRRRRRRRR..", ".RRRRRRRRRRRR.", "RYYRRRRRRRRYYR",
       "RRRRRRRRRRRRRR", ".KKK......KKK.", ".KKK......KKK."]


def race(frame=0, seed=5):
    img = canvas("N")
    horizon = 46
    # niebo z zachodem slonca (pasy jak na 8-bit)
    for y in range(horizon):
        t = y / horizon
        c = np.array(PAL["N"]) * (1 - t) + np.array(PAL["M"]) * t * 0.6
        img[y] = c.astype(np.uint8)
    for dy in range(-9, 1):
        w = int(math.sqrt(max(0, 81 - dy * dy)))
        if dy % 3 != 0:
            img[horizon + dy, 80 - w:80 + w] = PAL["Y"] if dy < -4 else PAL["O"]
    # gory na horyzoncie
    for x in range(LW):
        h = int(5 + 3 * math.sin(x / 9.0) + 2 * math.sin(x / 3.0))
        img[horizon - h:horizon, x] = PAL["P"]
    # droga pseudo-3D
    for y in range(horizon, LH):
        z = (y - horizon + 1)
        half = 6 + z * 1.6
        stripe = int((80 / z + frame * 2.5)) % 2
        img[y, :] = PAL["T"] if stripe else PAL["G"]                         # trawa
        x0, x1 = int(80 - half), int(80 + half)
        img[y, max(0, x0):min(LW, x1)] = PAL["S"] if stripe else PAL["D"]     # asfalt
        kerb = max(1, int(z * 0.2))
        img[y, max(0, x0 - kerb):max(0, x0)] = PAL["R"] if stripe else PAL["W"]
        img[y, min(LW, x1):min(LW, x1 + kerb)] = PAL["R"] if stripe else PAL["W"]
        if stripe:
            for lane in (-1 / 3, 1 / 3):
                lx = int(80 + lane * half)
                img[y, lx:lx + max(1, int(z * 0.08))] = PAL["W"]
    # cyfry w niebie (zolte z cieniem)
    def px(x, y, s, c, r, k):
        rect(img, x + 1, y + 1, s, s, "K")
        rect(img, x, y, s, s, "Y" if r < 4 else "O")
    big_digits(img, "23:47", 9, 3, px)
    text(img, 0, 1, DATE, "C", center=True)
    sprite(img, 66 + (12 if frame else 0), 88, CAR)
    text(img, 2, 98, "180KM/H", "W")
    return up(img)


# ================================================================ I. FAJERWERKI
def fireworks(frame=0, seed=6):
    rnd = random.Random(seed + frame)
    img = canvas("K")
    for _ in range(35):
        img[rnd.randrange(70), rnd.randrange(LW)] = PAL[rnd.choice("SDW")]
    # wybuchy
    bursts = [(28, 22, "M", 11), (130, 18, "C", 13)] if frame == 0 else [(56, 14, "Y", 14), (110, 28, "G", 10), (140, 12, "R", 6)]
    for cx, cy, col, r in bursts:
        for i in range(20):
            a = i / 20 * 2 * math.pi
            for k in range(r - 3, r + 1):
                x, y = int(cx + math.cos(a) * k), int(cy + math.sin(a) * k * 0.9)
                if 0 <= x < LW and 0 <= y < LH:
                    img[y, x] = PAL[col] if k > r - 2 else PAL["W"]
    # rakieta w locie
    rx, ry = (95, 50) if frame == 0 else (20, 40)
    img[ry:ry + 6, rx] = PAL["O"]
    img[ry - 1, rx] = PAL["W"]
    # cyfry (biale, gruby kontur fioletowy)
    def px(x, y, s, c, r, k):
        rect(img, x - 1, y - 1, s + 2, s + 2, "P")
    def px2(x, y, s, c, r, k):
        rect(img, x, y, s, s, "W")
    big_digits(img, "23:47", 34, 4, px)
    big_digits(img, "23:47", 34, 4, px2)
    # miasto: bloki z oknami
    x = 0
    rnd2 = random.Random(9)
    while x < LW:
        w, h = rnd2.randint(10, 22), rnd2.randint(14, 32)
        rect(img, x, LH - 10 - h, w, h, "D")
        for wy in range(LH - 8 - h, LH - 12, 4):
            for wx in range(x + 2, min(LW, x + w - 2), 3):
                if rnd2.random() < (0.35 if frame == 0 else 0.45):
                    img[wy, wx] = PAL[rnd2.choice("YYO")]
        x += w + 1
    rect(img, 0, LH - 10, LW, 10, "K")
    text(img, 0, LH - 9, DATE, "W", center=True)
    return up(img)


def main():
    OUT.mkdir(exist_ok=True)
    strip([bricks(0), bricks(1)]).save(OUT / "G_cegielki.png")
    strip([race(0), race(1)]).save(OUT / "H_wyscig.png")
    strip([fireworks(0), fireworks(1)]).save(OUT / "I_fajerwerki.png")
    print("zapisano", OUT)


if __name__ == "__main__":
    main()
