"""Makiety nowej tarczy cyberpunk (jaskrawe kolory na czarnym tle, duze animacje):
  A. Netrunner HUD   - paleta 2077, obracajace sie pierscienie, skaner, kolumny danych
  B. Klub / LED      - cyfry z diod w teczy, korektor graficzny, lasery
  C. Tunel predkosci - neonowy tunel na widza, smugi swiatel

  python tools/prerender/cyber_options/mockups.py
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
SS = 3
DATE = "PIĄTEK  02.10.2026"
BAYER4 = np.array([[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]])

YELLOW, CYAN, RED, MAGENTA = (252, 238, 10), (0, 240, 255), (255, 0, 60), (255, 40, 200)


def F(name, px, wght=None):
    f = ImageFont.truetype(str(FONTS / name), px)
    if wght:
        try:
            f.set_variation_by_axes([wght])
        except Exception:
            pass
    return f


def to565(a):
    t = np.tile(BAYER4, (H // 4 + 1, W // 4 + 1))[:H, :W][..., None] / 16.0
    s = np.array([8, 4, 8])
    return Image.fromarray(np.clip(np.floor(np.clip(a + t * s, 0, 255) / s) * s, 0, 255).astype(np.uint8))


def M(fn, blur=0):
    """Maska rysowana w 3x i zmniejszana (gladkie krawedzie)."""
    m = Image.new("L", (W * SS, H * SS), 0)
    fn(ImageDraw.Draw(m), SS)
    m = m.resize((W, H), Image.LANCZOS)
    if blur:
        m = m.filter(ImageFilter.GaussianBlur(blur))
    return np.asarray(m, np.float32)[..., None] / 255


def neon(a, fn, rgb, core=(255, 255, 255), radii=((7, 0.6), (2.5, 0.9)), core_k=0.8):
    for r, k in radii:
        a += M(fn, r) * np.array(rgb, np.float32) * k * 1.4
    m = M(fn)
    a[:] = a * (1 - m) + (np.array(rgb, np.float32) * (1 - core_k) + np.array(core, np.float32) * core_k) * m


def fill(a, fn, rgb, k=1.0):
    m = M(fn)
    a[:] = a * (1 - m * k) + np.array(rgb, np.float32) * m * k


def ctext(d, s, y, txt, f, x=None):
    b = f.getbbox(txt)
    xx = (W - (b[2] - b[0])) / 2 - b[0] if x is None else x
    d.text((xx * s, (y - b[1]) * s), txt, font=f.font_variant(size=int(f.size * s)) if False else f, fill=255)


def text_mask(txt, f, cx, y, anchor="mt"):
    """Tekst rysowany od razu w skali SS (czcionka powiekszona)."""
    def fn(d, s):
        fs = f.font_variant(size=f.size * s)
        d.text((cx * s, y * s), txt, font=fs, fill=255, anchor=anchor)
    return fn


# ================================================================ A. NETRUNNER HUD
def netrunner(phase=0.0, scan_y=90, glitch=False):
    rnd = random.Random(int(phase * 100) + 7)
    a = np.zeros((H, W, 3), np.float32)
    # siatka heksagonalna w tle (ledwo widoczna)
    def hexgrid(d, s):
        r = 14
        for row in range(-1, H // 21 + 2):
            for col in range(-1, W // 24 + 2):
                cx, cy = col * 24 + (row % 2) * 12, row * 21
                pts = [((cx + r * math.cos(math.pi / 3 * i + math.pi / 6)) * s,
                        (cy + r * math.sin(math.pi / 3 * i + math.pi / 6)) * s) for i in range(7)]
                d.line(pts, fill=255, width=s)
    a += M(hexgrid) * np.array([0, 40, 50]) * 0.6

    cx, cy = 240, 132
    # obracajace sie pierscienie segmentowe (za cyframi)
    def ring(d, s, r, w, segs, gap, rot):
        for i in range(segs):
            a0 = rot + i * 360 / segs
            d.arc([(cx - r) * s, (cy - r) * s, (cx + r) * s, (cy + r) * s], a0, a0 + 360 / segs - gap, fill=255, width=w * s)
    neon(a, lambda d, s: ring(d, s, 128, 4, 12, 9, phase * 40), CYAN, radii=((5, 0.5),), core_k=0.4)
    neon(a, lambda d, s: ring(d, s, 112, 2, 30, 6, -phase * 70), RED, radii=((4, 0.5),), core_k=0.3)
    neon(a, lambda d, s: ring(d, s, 144, 7, 3, 70, phase * 25 + 20), YELLOW, radii=((6, 0.6),), core_k=0.5)

    # kanciasta ramka pod cyframi (sciete rogi)
    x0, y0, x1, y1 = 74, 76, 406, 186
    plate = lambda d, s: d.polygon([((x0 + 16) * s, y0 * s), (x1 * s, y0 * s), (x1 * s, (y1 - 16) * s),
                                    ((x1 - 16) * s, y1 * s), (x0 * s, y1 * s), (x0 * s, (y0 + 16) * s)], fill=255)
    fill(a, plate, (6, 6, 10), 0.88)
    neon(a, lambda d, s: d.line([((x0 + 16) * s, y0 * s), (x1 * s, y0 * s), (x1 * s, (y1 - 16) * s), ((x1 - 16) * s, y1 * s),
                                 (x0 * s, y1 * s), (x0 * s, (y0 + 16) * s), ((x0 + 16) * s, y0 * s)], fill=255, width=2 * s),
         CYAN, radii=((4, 0.6),), core_k=0.5)
    fill(a, lambda d, s: d.rectangle([(x1 - 60) * s, (y1 - 3) * s, (x1 - 20) * s, (y1 + 3) * s], fill=255), RED)
    # cyfry
    f = F("Oxanium.ttf", 98, 800)
    neon(a, text_mask("23:47", f, cx, 132, "mm"), YELLOW, core=(255, 255, 210), radii=((8, 0.5), (3, 0.8)), core_k=0.55)
    neon(a, text_mask(DATE, F("Oxanium.ttf", 18, 600), cx, 204), CYAN, radii=((3, 0.5),), core_k=0.6)

    # kolumny danych po bokach
    fm = F("VT323.ttf", 15)
    for side, x in ((0, 8), (1, 418)):
        for i in range(17):
            y = 8 + i * 18 - int(phase * 30) % 18
            txt = "".join(rnd.choice("0123456789ABCDEF") for _ in range(6))
            col = YELLOW if rnd.random() < 0.15 else (0, 160, 180)
            neon(a, text_mask(txt, fm, x, y, "lt"), col, radii=((2, 0.3),), core_k=0.4)
    # etykiety HUD
    fl = F("Oxanium.ttf", 13, 600)
    neon(a, text_mask("NETRUNNER // LINK OK", fl, 74, 244, "lt"), RED, radii=((2, 0.4),), core_k=0.4)
    neon(a, text_mask("ICE 98%", fl, 406, 244, "rt"), CYAN, radii=((2, 0.4),), core_k=0.4)
    for i in range(10):  # pasek "obciazenia"
        on = i < 7 + int(2 * math.sin(phase * 3))
        fill(a, lambda d, s: d.rectangle([(74 + i * 14) * s, 266 * s, (84 + i * 14) * s, 276 * s], fill=255),
             YELLOW if on else (40, 40, 10))
    # pas skanera przez caly ekran
    yy = np.arange(H)[:, None]
    band = np.exp(-((yy - scan_y) / 6.0) ** 2)
    a += band[..., None] * np.array(CYAN) * 0.7
    a[int(scan_y)] = np.minimum(255, a[int(scan_y)] + np.array([150, 255, 255]))
    if glitch:  # przesuniete bloki z rozjechanym kolorem
        for _ in range(5):
            y, h = rnd.randint(60, 220), rnd.randint(4, 12)
            sh = rnd.choice([-1, 1]) * rnd.randint(10, 30)
            a[y:y + h] = np.roll(a[y:y + h], sh, axis=1)
            a[y:y + h, :, 0] = np.roll(a[y:y + h, :, 0], 6, axis=1)
    return to565(a)


# ================================================================ B. KLUB / LED
DOTS = {  # 5x7
    "0": ["01110", "10001", "10011", "10101", "11001", "10001", "01110"],
    "2": ["01110", "10001", "00001", "00010", "00100", "01000", "11111"],
    "3": ["11110", "00001", "00001", "01110", "00001", "00001", "11110"],
    "4": ["00010", "00110", "01010", "10010", "11111", "00010", "00010"],
    "7": ["11111", "00001", "00010", "00100", "01000", "01000", "01000"],
    ":": ["0", "1", "1", "0", "1", "1", "0"],
}


def rainbow(t):
    stops = [(0.0, MAGENTA), (0.33, (140, 60, 255)), (0.66, CYAN), (1.0, YELLOW)]
    for (t0, c0), (t1, c1) in zip(stops, stops[1:]):
        if t <= t1:
            u = (t - t0) / (t1 - t0)
            return tuple(c0[i] + (c1[i] - c0[i]) * u for i in range(3))
    return stops[-1][1]


def club(phase=0.0, eq_seed=1):
    rnd = random.Random(eq_seed)
    a = np.zeros((H, W, 3), np.float32)
    # lasery z gornych rogow
    for i, (ox, col) in enumerate(((0, MAGENTA), (480, CYAN), (240, (120, 255, 60)))):
        ang = math.radians(60 + 40 * math.sin(phase * 1.3 + i * 2.1)) if ox == 0 else (
            math.radians(120 + 40 * math.sin(phase * 1.1 + i)) if ox == 480 else math.radians(90 + 30 * math.sin(phase * 1.7)))
        ex, ey = ox + math.cos(ang) * 700, -10 + math.sin(ang) * 700
        neon(a, lambda d, s: d.line([(ox * s, -10 * s), (ex * s, ey * s)], fill=255, width=2 * s), col,
             radii=((5, 0.35), (2, 0.6)), core_k=0.6)
    a *= 0.75
    # cyfry z diod
    text = "23:47"
    pitch, r = 11, 4.2
    widths = [len(DOTS[c][0]) for c in text]
    total = sum(w * pitch for w in widths) + pitch * (len(text) - 1)
    x = (W - total) / 2
    for c, w in zip(text, widths):
        for row, line in enumerate(DOTS[c]):
            for col, bit in enumerate(line):
                px, py = x + col * pitch + r, 52 + row * pitch * 1.25
                t = (px - 80) / 320
                rgb = rainbow(min(1, max(0, t + 0.15 * math.sin(phase * 2))))
                if bit == "1":
                    neon(a, lambda d, s: d.ellipse([(px - r) * s, (py - r) * s, (px + r) * s, (py + r) * s], fill=255), rgb,
                         radii=((4, 0.5),), core_k=0.35)
                else:
                    fill(a, lambda d, s: d.ellipse([(px - r) * s, (py - r) * s, (px + r) * s, (py + r) * s], fill=255), (22, 18, 30))
        x += w * pitch + pitch
    neon(a, text_mask(DATE, F("Rajdhani-Bold.ttf", 20), 240, 150), (255, 255, 255), radii=((3, 0.3),), core_k=0.9)
    # korektor graficzny
    bars = 24
    bw = W / bars
    for i in range(bars):
        h = int(18 + 110 * abs(math.sin(i * 0.55 + phase * 3.1)) * rnd.uniform(0.5, 1.0))
        segs = h // 8
        for k in range(segs):
            y1 = 312 - k * 8
            t = k / 16
            col = (120, 255, 60) if t < 0.5 else (YELLOW if t < 0.8 else RED)
            fill(a, lambda d, s: d.rectangle([(i * bw + 3) * s, (y1 - 5) * s, ((i + 1) * bw - 3) * s, y1 * s], fill=255), col)
        a += M(lambda d, s: d.rectangle([(i * bw + 2) * s, (312 - segs * 8) * s, ((i + 1) * bw - 2) * s, 314 * s], fill=255), 5) * np.array([60, 40, 60]) * 0.5
        peak = 312 - segs * 8 - 10
        fill(a, lambda d, s: d.rectangle([(i * bw + 3) * s, (peak - 2) * s, ((i + 1) * bw - 3) * s, peak * s], fill=255), (255, 255, 255))
    return to565(a)


# ================================================================ C. TUNEL
def tunnel(phase=0.0, seed=3):
    rnd = random.Random(seed)
    a = np.zeros((H, W, 3), np.float32)
    vx, vy = 240, 150
    # prostokaty tunelu (perspektywa 1/z), przesuwaja sie na widza
    for k in range(14):
        z = (k + 1 - phase % 1.0) * 0.55
        if z < 0.2:
            continue
        sx, sy = 300 / z, 200 / z
        col = MAGENTA if (k + int(phase)) % 2 == 0 else CYAN
        bright = min(1.0, 0.25 + 0.9 / z)
        neon(a, lambda d, s: d.rectangle([(vx - sx) * s, (vy - sy) * s, (vx + sx) * s, (vy + sy) * s], outline=255, width=max(1, int(2 * s / max(z, 0.6)))),
             tuple(c * bright for c in col), radii=((4, 0.4),), core_k=0.4)
    # linie zbiegu w rogach
    for ex, ey in ((0, 0), (W, 0), (0, H), (W, H)):
        neon(a, lambda d, s: d.line([(vx * s, vy * s), (ex * s, ey * s)], fill=255, width=s), (90, 40, 140), radii=((3, 0.3),), core_k=0.3)
    # smugi swiatel (pojazdy) wylatujace z centrum
    for i in range(14):
        ang = rnd.uniform(0, 2 * math.pi)
        d0 = (rnd.uniform(0.1, 1.0) + phase * 0.6) % 1.0
        r0, r1 = 30 + d0 ** 2 * 380, 30 + min(1.0, d0 + 0.12) ** 2 * 380
        col = rnd.choice([YELLOW, RED, (255, 255, 255), CYAN])
        neon(a, lambda d, s: d.line([((vx + math.cos(ang) * r0) * s, (vy + math.sin(ang) * r0 * 0.7) * s),
                                      ((vx + math.cos(ang) * r1) * s, (vy + math.sin(ang) * r1 * 0.7) * s)], fill=255, width=2 * s),
             col, radii=((3, 0.6),), core_k=0.7)
    # ciemna tablica pod cyframi
    fill(a, lambda d, s: d.rounded_rectangle([110 * s, 100 * s, 370 * s, 200 * s], radius=14 * s, fill=255), (4, 2, 10), 0.85)
    neon(a, lambda d, s: d.rounded_rectangle([110 * s, 100 * s, 370 * s, 200 * s], radius=14 * s, outline=255, width=2 * s),
         YELLOW, radii=((4, 0.5),), core_k=0.5)
    neon(a, text_mask("23:47", F("Rajdhani-Bold.ttf", 104), 240, 150, "mm"), CYAN, core=(230, 255, 255),
         radii=((8, 0.6), (3, 0.9)), core_k=0.6)
    neon(a, text_mask(DATE, F("Rajdhani-Bold.ttf", 20), 240, 214), MAGENTA, radii=((3, 0.5),), core_k=0.6)
    return to565(a)


def strip(frames):
    s = Image.new("RGB", (W * len(frames) + 8 * (len(frames) - 1), H), (0, 0, 0))
    for i, f in enumerate(frames):
        s.paste(f, (i * (W + 8), 0))
    return s


def main():
    OUT.mkdir(exist_ok=True)
    strip([netrunner(0.0, 70), netrunner(1.3, 210, glitch=True)]).save(OUT / "A_netrunner.png")
    strip([club(0.0, 1), club(1.0, 5)]).save(OUT / "B_klub.png")
    strip([tunnel(0.0, 3), tunnel(0.5, 3)]).save(OUT / "C_tunel.png")
    print("zapisano", OUT)


if __name__ == "__main__":
    main()
