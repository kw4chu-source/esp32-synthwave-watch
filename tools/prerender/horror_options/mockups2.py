"""Druga seria opcji horroru - analogowy, realistyczny styl (bez kiczu):
  A. transmisja awaryjna (komunikat EAS na niebieskiej planszy)
  B. monitoring (4 kamery nocne, w jednej czasem cos sie pojawia)
  C. okno nocą (latarnia na ulicy, ktos pod nia stoi; odbicie budzika w szybie)

  python tools/prerender/horror_options/mockups2.py
"""

from __future__ import annotations

import math
import random
import sys
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont

HERE = Path(__file__).parent
sys.path.insert(0, str(HERE.parent / "vhs_hallway"))
import mockup as mk  # noqa: E402  (korytarz do kamery 1)

FONTS = HERE.parent / "fonts"
OUT = HERE / "out"
W, H = 480, 320
BAYER4 = np.array([[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]])


def vt(px):
    return ImageFont.truetype(str(FONTS / "VT323.ttf"), px)


def to565(img):
    h, w = img.shape[:2]
    t = np.tile(BAYER4, (h // 4 + 1, w // 4 + 1))[:h, :w][..., None] / 16.0
    steps = np.array([8, 4, 8])
    return Image.fromarray(np.clip(np.floor(np.clip(img + t * steps, 0, 255) / steps) * steps, 0, 255).astype(np.uint8))


def m_of(fn, size=(W, H), blur=0):
    m = Image.new("L", size, 0)
    fn(ImageDraw.Draw(m))
    if blur:
        m = m.filter(ImageFilter.GaussianBlur(blur))
    return np.asarray(m, np.float32)[..., None] / 255


def over(a, m, rgb, k=1.0):
    a[:] = a * (1 - m * k) + np.array(rgb, np.float32) * m * k


def grain(a, sigma, seed):
    a += np.random.default_rng(seed).normal(0, sigma, a.shape[:2])[..., None]


def scan(a, k=0.88):
    a[::2] *= k


# ================================================================ A. TRANSMISJA AWARYJNA
LINES = [
    "ZOSTAŃ W DOMU. ZAMKNIJ DRZWI I OKNA.",
    "NIE PATRZ NA NIEBO PO ZMIERZCHU.",
    "JEŚLI KTOŚ ZAPUKA - NIE OTWIERAJ,",
    "NAWET JEŚLI ZNASZ TEN GŁOS.",
]


def broadcast(bars=False, seed=1):
    a = np.zeros((H, W, 3), np.float32)
    if bars:
        cols = [(192, 192, 192), (192, 192, 0), (0, 192, 192), (0, 192, 0), (192, 0, 192), (192, 0, 0), (0, 0, 192)]
        bw = W / 7
        for i, c in enumerate(cols):
            a[:210, int(i * bw):int((i + 1) * bw)] = c
        a[210:240] = (20, 20, 20)
        for i, c in enumerate([(0, 0, 192), (19, 19, 19), (192, 0, 192), (19, 19, 19), (0, 192, 192), (19, 19, 19), (192, 192, 192)]):
            a[210:240, int(i * bw):int((i + 1) * bw)] = c
        a[240:] = (16, 16, 16)
        t = m_of(lambda d: d.text((W / 2 - vt(40).getlength("BRAK SYGNAŁU") / 2, 254), "BRAK SYGNAŁU", font=vt(40), fill=255))
        over(a, t, (230, 230, 230))
        # przesuniety blok obrazu
        a[90:110] = np.roll(a[90:110], 37, axis=1)
    else:
        yy = np.arange(H)[:, None, None]
        a[:] = np.array([8, 22, 120]) * (1 - yy / H * 0.35)
        # godlo / znak sluzby (prosty, urzedowy)
        def emblem(d):
            d.ellipse([20, 18, 76, 74], outline=255, width=3)
            d.polygon([(48, 26), (66, 62), (30, 62)], outline=255, width=3)
            d.line([(48, 40), (48, 54)], fill=255, width=3)
            d.ellipse([46, 57, 50, 61], fill=255)
        over(a, m_of(emblem), (235, 235, 240))
        over(a, m_of(lambda d: d.text((88, 18), "SYSTEM OSTRZEGANIA LUDNOŚCI", font=vt(26), fill=255)), (235, 235, 240))
        over(a, m_of(lambda d: d.text((88, 42), "KOMUNIKAT NR 7 / KANAŁ 03", font=vt(22), fill=255)), (170, 180, 230))
        # godzina - duza, urzedowa
        f = vt(110)
        t = "23:47"
        over(a, m_of(lambda d: d.text(((W - f.getlength(t)) / 2, 54), t, font=f, fill=255)), (245, 245, 250))
        over(a, m_of(lambda d: d.text((W / 2 - vt(24).getlength("PIĄTEK 02.10.2026") / 2, 166), "PIĄTEK 02.10.2026", font=vt(24), fill=255)), (180, 190, 235))
        over(a, m_of(lambda d: d.line([(20, 196), (W - 20, 196)], fill=255, width=2)), (200, 200, 230))
        for i, ln in enumerate(LINES):
            over(a, m_of(lambda d: d.text((22, 202 + i * 22), ln, font=vt(23), fill=255)), (245, 245, 250))
        # pasek przewijany na dole
        a[294:] = (150, 10, 10)
        over(a, m_of(lambda d: d.text((-40, 294), "  NIE WYCHODŹ  •  NIE ODPOWIADAJ  •  NIE PATRZ  •  NIE WYCHODŹ", font=vt(24), fill=255)), (255, 240, 240))
    grain(a, 6, seed)
    scan(a)
    return to565(a)


# ================================================================ B. MONITORING
def cam_corridor():
    arr = mk.corridor(None)
    img = Image.fromarray(np.clip(arr, 0, 255).astype(np.uint8)).resize((240, 160), Image.BILINEAR)
    return np.asarray(img, np.float32)


def cam_scene(kind, figure=False, seed=0):
    r = random.Random(seed)
    w, h = 240, 160
    a = np.zeros((h, w, 3), np.float32)
    if kind == "corridor":
        a = cam_corridor()
    elif kind == "stairs":
        a[:] = 50
        d_m = lambda fn: m_of(fn, (w, h))
        for i in range(9):
            y0 = 150 - i * 15
            x0 = 30 + i * 9
            over(a, d_m(lambda d: d.rectangle([x0, y0 - 15, 230 - i * 6, y0], fill=255)), (70 + i * 6,) * 3)
            over(a, d_m(lambda d: d.line([(x0, y0 - 15), (230 - i * 6, y0 - 15)], fill=255)), (110,) * 3)
        over(a, d_m(lambda d: d.rectangle([0, 0, 40, h], fill=255)), (30,) * 3)
        over(a, d_m(lambda d: d.line([(60, 150), (150, 20)], fill=255, width=3)), (20,) * 3)  # porecz
        a *= np.linspace(1.1, 0.4, h)[:, None, None]
    elif kind == "door":
        a[:] = 40
        d_m = lambda fn: m_of(fn, (w, h))
        over(a, d_m(lambda d: d.rectangle([70, 14, 170, 160], fill=255)), (60,) * 3)
        over(a, d_m(lambda d: d.rectangle([80, 22, 160, 160], fill=255)), (85,) * 3)
        over(a, d_m(lambda d: d.rectangle([100, 34, 140, 80], fill=255)), (150,) * 3)   # matowa szybka
        over(a, d_m(lambda d: d.ellipse([146, 92, 154, 100], fill=255)), (180,) * 3)
        if figure:  # sylwetka za matowa szyba
            over(a, d_m(lambda d: (d.ellipse([110, 38, 130, 60], fill=255), d.rectangle([104, 56, 136, 82], fill=255))) * 1.0, (40,) * 3, 0.85)
        over(a, d_m(lambda d: d.rectangle([0, 140, w, h], fill=255)), (55,) * 3)
    elif kind == "basement":
        a[:] = 14
        yy, xx = np.mgrid[0:h, 0:w]
        a += (np.exp(-(((xx - 150) / 70) ** 2 + ((yy - 40) / 60) ** 2)) * 120)[..., None]
        d_m = lambda fn: m_of(fn, (w, h))
        over(a, d_m(lambda d: d.line([(150, 0), (150, 26)], fill=255)), (30,) * 3)
        over(a, m_of(lambda d: d.ellipse([144, 24, 156, 38], fill=255), (w, h), 1), (230,) * 3)
        over(a, d_m(lambda d: (d.rectangle([20, 70, 80, 150], fill=255), d.rectangle([180, 90, 236, 150], fill=255))), (24,) * 3)
        over(a, d_m(lambda d: d.rectangle([0, 140, w, h], fill=255)), (30,) * 3)
    # kamera nocna: szarosc z lekka zielenia, szum, winieta
    g = a.mean(axis=2, keepdims=True)
    a = np.concatenate([g * 0.86, g * 1.0, g * 0.88], axis=2)
    a += np.random.default_rng(seed).normal(0, 10, (h, w, 1))
    yy, xx = np.mgrid[0:h, 0:w]
    a *= (1 - 0.5 * (np.hypot((xx - w / 2) / (w / 1.4), (yy - h / 2) / (h / 1.3))) ** 2)[..., None]
    return a


def monitoring(variant=0):
    a = np.zeros((H, W, 3), np.float32)
    panes = [("corridor", "CAM 1  KORYTARZ"), ("stairs", "CAM 2  SCHODY"),
             ("door", "CAM 3  DRZWI"), ("basement", "CAM 4  PIWNICA")]
    for i, (kind, label) in enumerate(panes):
        x0, y0 = (i % 2) * 240, (i // 2) * 160
        if variant == 1 and kind == "basement":
            p = np.random.default_rng(9).normal(70, 45, (160, 240, 1)).repeat(3, axis=2)
            p = np.clip(p, 0, 255)
            pm = m_of(lambda d: d.text((120 - vt(30).getlength("BRAK SYGNAŁU") / 2, 64), "BRAK SYGNAŁU", font=vt(30), fill=255), (240, 160))
            p = p * (1 - pm) + 235 * pm
        else:
            p = cam_scene(kind, figure=(variant == 1 and kind == "door"), seed=i + 3 * variant)
        lm = m_of(lambda d: d.text((6, 2), label, font=vt(18), fill=255), (240, 160))
        p = p * (1 - lm) + 225 * lm
        a[y0:y0 + 160, x0:x0 + 240] = p
    a[:, 238:242] = 0
    a[158:162] = 0
    # godzina w srodku, na ciemnym polu
    f = vt(78)
    t = "23:47"
    tw = f.getlength(t)
    a[124:198, int(W / 2 - tw / 2 - 14):int(W / 2 + tw / 2 + 14)] *= 0.15
    over(a, m_of(lambda d: d.text((W / 2 - tw / 2, 116), t, font=f, fill=255)), (235, 240, 235))
    ds = "PIĄTEK 02.10.2026"
    over(a, m_of(lambda d: d.text((W / 2 - vt(18).getlength(ds) / 2, 182), ds, font=vt(18), fill=255)), (170, 180, 170))
    rec = m_of(lambda d: d.ellipse([W - 22, 6, W - 12, 16], fill=255))
    over(a, rec, (220, 30, 30))
    scan(a, 0.9)
    return to565(a)


# ================================================================ C. OKNO NOCA
def window(lamp_on=True, figure_pos=(330, 214, 0.75), seed=3):
    r = random.Random(seed)
    a = np.zeros((H, W, 3), np.float32)
    yy, xx = np.mgrid[0:H, 0:W]
    a[:] = np.array([6, 8, 14])[None, None, :] + (yy / H * 10)[..., None]
    # domy po drugiej stronie ulicy
    def houses(d):
        d.polygon([(0, 190), (0, 120), (60, 92), (120, 120), (120, 190)], fill=255)
        d.polygon([(150, 190), (150, 130), (215, 100), (280, 130), (280, 190)], fill=255)
        d.polygon([(400, 190), (400, 112), (450, 88), (500, 112), (500, 190)], fill=255)
    over(a, m_of(houses), (12, 13, 18))
    over(a, m_of(lambda d: d.rectangle([186, 146, 204, 162], fill=255)), (150, 120, 60))   # jedno zapalone okno
    # ulica, mokry asfalt
    a[190:] = np.array([10, 10, 13]) + (yy[190:] - 190)[..., None] * 0.08
    # latarnia sodowa
    lx = 330
    over(a, m_of(lambda d: (d.line([(lx, 200), (lx, 70)], fill=255, width=3), d.line([(lx, 72), (lx - 26, 72)], fill=255, width=3))), (20, 20, 22))
    if lamp_on:
        cone = np.clip(1 - np.abs(xx - (lx - 24)) / (20 + (yy - 74).clip(0) * 0.55), 0, 1) * (yy > 74) * (yy < 260)
        a += (cone * 0.35)[..., None] * np.array([255, 150, 60])
        pool = np.exp(-(((xx - (lx - 24)) / 70) ** 2 + ((yy - 228) / 16) ** 2))
        a += (pool * 0.7)[..., None] * np.array([255, 140, 50])
        glow = np.exp(-(((xx - (lx - 24)) / 14) ** 2 + ((yy - 76) / 9) ** 2))
        a += glow[..., None] * np.array([255, 190, 110]) * 1.4
    # postac pod latarnia / blizej
    fx, fy, s = figure_pos
    fig = m_of(lambda d: (d.ellipse([fx - 5 * s, fy - 62 * s, fx + 5 * s, fy - 50 * s], fill=255),
                          d.polygon([(fx - 7 * s, fy - 50 * s), (fx + 7 * s, fy - 50 * s), (fx + 6 * s, fy), (fx - 6 * s, fy)], fill=255)), blur=0.8)
    over(a, fig, (3, 3, 4), 0.95)
    # odbicie budzika w szybie (czerwone 7-seg, rozmyte)
    f = ImageFont.truetype(str(FONTS / "VT323.ttf"), 96)
    t = "23:47"
    tm = m_of(lambda d: d.text(((W - f.getlength(t)) / 2, 6), t, font=f, fill=255), blur=0.6)
    tg = m_of(lambda d: d.text(((W - f.getlength(t)) / 2, 6), t, font=f, fill=255), blur=6)
    a += tg * np.array([200, 20, 10]) * 0.6
    a = a * (1 - tm * 0.85) + np.array([255, 45, 30]) * tm * 0.85
    ds = "PIĄTEK 02.10.2026"
    dm = m_of(lambda d: d.text(((W - vt(20).getlength(ds)) / 2, 104), ds, font=vt(20), fill=255), blur=0.4)
    a = a * (1 - dm * 0.6) + np.array([220, 60, 40]) * dm * 0.6
    # krople na szybie: rozmyte kropki + splywajace smugi
    drops = Image.new("L", (W, H), 0)
    dd = ImageDraw.Draw(drops)
    for _ in range(140):
        x, y, rr = r.uniform(0, W), r.uniform(0, H), r.uniform(1, 3.2)
        dd.ellipse([x - rr, y - rr, x + rr, y + rr * 1.2], fill=r.randint(60, 140))
    for _ in range(14):
        x, y = r.uniform(0, W), r.uniform(0, H * 0.6)
        dd.line([(x, y), (x + r.uniform(-3, 3), y + r.uniform(30, 90))], fill=90, width=2)
    dm2 = np.asarray(drops.filter(ImageFilter.GaussianBlur(0.8)), np.float32)[..., None] / 255
    a = a + dm2 * 55
    # rama okna (krzyz) w ciemnym pierwszym planie
    frame = m_of(lambda d: (d.rectangle([0, 0, W, 6], fill=255), d.rectangle([0, H - 8, W, H], fill=255),
                            d.rectangle([0, 0, 8, H], fill=255), d.rectangle([W - 8, 0, W, H], fill=255),
                            d.rectangle([W // 2 - 5, 0, W // 2 + 5, H], fill=255) if False else None,
                            d.rectangle([0, 150, W, 158], fill=255) if False else None))
    over(a, frame, (4, 4, 5))
    grain(a, 5, seed)
    a *= (1 - 0.5 * (np.hypot((xx - W / 2) / (W / 1.5), (yy - H / 2) / (H / 1.3))) ** 2)[..., None]
    return to565(a)


def strip(frames):
    s = Image.new("RGB", (W * len(frames) + 8 * (len(frames) - 1), H), (0, 0, 0))
    for i, f in enumerate(frames):
        s.paste(f, (i * (W + 8), 0))
    return s


def main():
    OUT.mkdir(exist_ok=True)
    strip([broadcast(), broadcast(bars=True)]).save(OUT / "A_transmisja.png")
    strip([monitoring(0), monitoring(1)]).save(OUT / "B_monitoring.png")
    strip([window(), window(lamp_on=False, figure_pos=(262, 262, 1.45))]).save(OUT / "C_okno.png")
    print("zapisano", OUT)


if __name__ == "__main__":
    main()
