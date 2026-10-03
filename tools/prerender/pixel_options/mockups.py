"""Makiety tarczy w stylu 8-bit (rysowane w 160x107, powiekszane x3 bez wygladzania):
  D. Najezdzcy  - kosmici maszeruja, statek strzela, cyfry sa oslonami (obtlukiwane)
  E. Platformowka - bohater biegnie i skacze, paralaksa, monety, przeciwnicy
  F. Spadajace klocki - cyfry ukladane z klockow, nowy klocek spada przy zmianie minuty

  python tools/prerender/pixel_options/mockups.py
"""

from __future__ import annotations

import random
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFont

HERE = Path(__file__).parent
FONTS = HERE.parent / "fonts"
OUT = HERE / "out"
LW, LH = 160, 107  # niska rozdzielczosc
DATE = "PIĄTEK 02.10.2026"

PAL = {
    "K": (0, 0, 0), "N": (12, 10, 34), "W": (252, 252, 252), "C": (0, 232, 216), "M": (248, 56, 152),
    "Y": (248, 216, 0), "G": (0, 200, 60), "R": (232, 40, 24), "O": (248, 120, 0), "B": (40, 80, 248),
    "P": (152, 72, 248), "S": (110, 110, 140), "D": (40, 36, 70), "L": (130, 200, 255), "T": (0, 120, 40),
}

FONT5x7 = {
    "0": ["01110", "10001", "10011", "10101", "11001", "10001", "01110"],
    "1": ["00100", "01100", "00100", "00100", "00100", "00100", "01110"],
    "2": ["01110", "10001", "00001", "00010", "00100", "01000", "11111"],
    "3": ["11110", "00001", "00001", "01110", "00001", "00001", "11110"],
    "4": ["00010", "00110", "01010", "10010", "11111", "00010", "00010"],
    "7": ["11111", "00001", "00010", "00100", "01000", "01000", "01000"],
    ":": ["0", "0", "1", "0", "1", "0", "0"],
}


def canvas(bg="K"):
    return np.full((LH, LW, 3), PAL[bg], np.uint8)


def sprite(img, x, y, rows, color=None):
    for j, row in enumerate(rows):
        for i, ch in enumerate(row):
            if ch == "." or ch == " ":
                continue
            px, py = x + i, y + j
            if 0 <= px < LW and 0 <= py < LH:
                img[py, px] = PAL[color or ch] if (color or ch) in PAL else PAL["W"]


def rect(img, x, y, w, h, c):
    img[max(0, y):max(0, min(LH, y + h)), max(0, x):max(0, min(LW, x + w))] = PAL[c] if isinstance(c, str) else c


def text(img, x, y, s, c="W", center=False):
    f = ImageFont.truetype(str(FONTS / "PressStart2P-Regular.ttf"), 8)
    m = Image.new("1", (LW, LH), 0)
    d = ImageDraw.Draw(m)
    d.fontmode = "1"
    if center:
        x = (LW - d.textlength(s, font=f)) // 2
    d.text((x, y), s, font=f, fill=1)
    img[np.asarray(m, bool)] = PAL[c]


def big_digits(img, txt, y, scale, draw_px):
    """Cyfry 5x7, kazdy piksel czcionki = blok scale x scale (rysowany przez draw_px)."""
    widths = [len(FONT5x7[c][0]) for c in txt]
    total = sum(w * scale for w in widths) + scale * (len(txt) - 1)
    x = (LW - total) // 2
    for c, w in zip(txt, widths):
        for r, row in enumerate(FONT5x7[c]):
            for k, bit in enumerate(row):
                if bit == "1":
                    draw_px(x + k * scale, y + r * scale, scale, c, r, k)
        x += w * scale + scale


def up(img):
    return Image.fromarray(img).resize((LW * 3, LH * 3), Image.NEAREST).crop((0, 0, 480, 320))


# ================================================================ D. NAJEZDZCY
ALIEN_A = [["..X...X..", "...XXX...", "..XXXXX..", ".XX.X.XX.", "XXXXXXXXX", "X.X...X.X", "..X...X.."],
           ["..X...X..", "...XXX...", "..XXXXX..", ".XX.X.XX.", "XXXXXXXXX", ".XX...XX.", "X.......X"]]
ALIEN_B = [["...XX...", "..XXXX..", ".XXXXXX.", "XX.XX.XX", "XXXXXXXX", ".X.XX.X.", "X......X"],
           ["...XX...", "..XXXX..", ".XXXXXX.", "XX.XX.XX", "XXXXXXXX", "..X..X..", ".X....X."]]
SHIP = ["....XXX....", "...XXXXX...", ".XXXXXXXXX.", "XXXXXXXXXXX", "XXX.XXX.XXX"]
UFO = ["....XXXXX....", "..XXXXXXXXX..", ".XX.XX.XX.XX.", "XXXXXXXXXXXXX", "..XXX...XXX.."]
BOOM = ["X...X...X", ".X..X..X.", "..X...X..", "XX.....XX", "..X...X..", ".X..X..X.", "X...X...X"]


def invaders(frame=0, seed=1):
    rnd = random.Random(seed)
    img = canvas("K")
    for _ in range(40):  # gwiazdy
        img[rnd.randrange(LH), rnd.randrange(LW)] = PAL[rnd.choice("SDW")]
    text(img, 4, 2, "SCORE 2347", "W")
    text(img, 100, 2, "HI 9999", "Y")
    off = 3 if frame else 0
    for row, (spr, col) in enumerate(((ALIEN_A, "M"), (ALIEN_B, "C"), (ALIEN_A, "G"))):
        for i in range(8):
            x, y = 22 + i * 15 + off, 13 + row * 9
            if frame and row == 1 and i == 5:
                sprite(img, x, y, BOOM, "Y")
            else:
                sprite(img, x, y, spr[frame], col)
    if frame:
        sprite(img, 140, 11 - 2, UFO, "R")
    # cyfry = oslony (z wybitymi dziurami)
    holes = {(rnd.randrange(160), rnd.randrange(45, 75)) for _ in range(60)} if frame else set()
    def px(x, y, s, c, r, k):
        for yy in range(y, y + s):
            for xx in range(x, x + s):
                if (xx, yy) not in holes:
                    img[yy, xx] = PAL["G"] if r < 5 else PAL["T"]
    big_digits(img, "23:47", 45, 4, px)
    # pociski
    rect(img, 81, 82, 1, 4, "W")
    rect(img, 50, 40 if frame else 36, 1, 3, "M")
    if frame:
        rect(img, 112, 60, 1, 3, "C")
    sprite(img, 76 + (6 if frame else 0), 90, SHIP, "C")
    rect(img, 0, 97, LW, 1, "G")
    text(img, 0, 99, DATE, "W", center=True)
    return up(img)


# ================================================================ E. PLATFORMOWKA
HERO = [["..RRRR..", ".RRRRRR.", ".WWOWOW.", ".OOOOOO.", "..BBBB..", ".BBYBBB.", "B.BBBB.B", "..B..B..", ".BB..BB."],
        ["..RRRR..", ".RRRRRR.", ".WWOWOW.", ".OOOOOO.", "..BBBB..", "BBBYBBB.", "..BBBB.B", ".B....B.", "B......B"]]
SLIME = ["..GGGG..", ".GGGGGG.", "GWKGGWKG", "GGGGGGGG", "GGGGGGGG"]
COIN = [".YY.", "YWYY", "YWYY", ".YY."]
CLOUD = ["...WWW......", ".WWWWWWW.WW.", "WWWWWWWWWWWW", ".WWWWWWWWWW."]


def platformer(frame=0, seed=2):
    rnd = random.Random(seed)
    img = canvas("N")
    for _ in range(30):
        img[rnd.randrange(60), rnd.randrange(LW)] = PAL[rnd.choice("SWD")]
    # ksiezyc i chmury
    for dy in range(-5, 6):
        for dx in range(-5, 6):
            if dx * dx + dy * dy <= 25:
                img[16 + dy, 136 + dx] = PAL["Y"]
    sprite(img, 10 - frame * 2, 8, CLOUD, "S")
    sprite(img, 100 - frame * 2, 14, CLOUD, "S")
    # wzgorza (paralaksa)
    for x in range(LW):
        h = int(10 + 6 * np.sin((x + frame * 3) / 14.0) + 4 * np.sin((x + frame * 3) / 5.0))
        img[86 - h:86, x] = PAL["P"]
    for x in range(LW):
        h = int(6 + 4 * np.sin((x + frame * 6) / 9.0))
        img[86 - h:86, x] = PAL["D"]
    # ziemia z cegiel
    rect(img, 0, 86, LW, 2, "G")
    for y in range(88, LH):
        for x in range(LW):
            xx = (x + frame * 4 + (4 if (y - 88) // 4 % 2 else 0)) % 8
            img[y, x] = PAL["K"] if (y - 88) % 4 == 3 or xx == 7 else PAL["O"]
    # cyfry w niebie (biale z cieniem)
    def px(x, y, s, c, r, k):
        rect(img, x + 1, y + 1, s, s, "B")
        rect(img, x, y, s, s, "W")
    big_digits(img, "23:47", 22, 4, px)
    text(img, 0, 2, DATE, "C", center=True)
    # bohater, przeciwnik, monety
    hy = 66 if frame else 77
    sprite(img, 40, hy, HERO[frame])
    sprite(img, 60 - frame * 4, 81, SLIME)
    for i, cx in enumerate((84, 94, 104)):
        if not (frame and i == 0):
            sprite(img, cx - frame * 4, 70, COIN)
    if frame:
        text(img, 74, 58, "+1", "Y")
    return up(img)


# ================================================================ F. SPADAJACE KLOCKI
BLOCK_COLORS = ["C", "M", "Y", "G", "O", "B", "R"]


def block(img, x, y, s, base):
    c = np.array(PAL[base], np.int32)
    light, dark = np.clip(c + 90, 0, 255), np.clip(c - 90, 0, 255)
    img[y:y + s, x:x + s] = c
    img[y, x:x + s] = light
    img[y:y + s, x] = light
    img[y + s - 1, x:x + s] = dark
    img[y:y + s, x + s - 1] = dark


def blocks(frame=0, seed=3):
    rnd = random.Random(seed)
    img = canvas("K")
    for y in range(0, LH, 5):  # przygaszona siatka planszy
        img[y, :] = PAL["D"] if y % 10 == 0 else img[y, :]
    for x in range(0, LW, 5):
        img[:, x] = np.maximum(img[:, x], np.array(PAL["N"]))
    colors = {}

    def px(x, y, s, c, r, k):
        key = (c, r, k)
        colors.setdefault(key, BLOCK_COLORS[(r * 3 + k + ord(c)) % 7])
        block(img, x, y, s, colors[key])
    big_digits(img, "23:47", 22, 5, px)
    # spadajacy klocek (L) nad ostatnia cyfra
    py = 4 if frame == 0 else 12
    for dx, dy in ((0, 0), (0, 1), (0, 2), (1, 2)):
        block(img, 118 + dx * 5, py + dy * 5, 5, "O")
    if frame:  # blysk kasowanej linii
        img[72:77, 8:152] = PAL["W"]
    rect(img, 0, 84, LW, 1, "S")
    text(img, 4, 88, "LINIE 1347", "Y")
    text(img, 100, 88, "LV 23", "C")
    text(img, 0, 98, DATE, "W", center=True)
    return up(img)


def strip(frames):
    s = Image.new("RGB", (480 * len(frames) + 8 * (len(frames) - 1), 320), (0, 0, 0))
    for i, f in enumerate(frames):
        s.paste(f, (i * 488, 0))
    return s


def main():
    OUT.mkdir(exist_ok=True)
    strip([invaders(0), invaders(1)]).save(OUT / "D_najezdzcy.png")
    strip([platformer(0), platformer(1)]).save(OUT / "E_platformowka.png")
    strip([blocks(0), blocks(1)]).save(OUT / "F_klocki.png")
    print("zapisano", OUT)


if __name__ == "__main__":
    main()
