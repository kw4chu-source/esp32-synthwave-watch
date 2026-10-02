"""Trzecia seria opcji horroru - "urzadzenie pokazuje cos, czego nie powinno":
  D. stacja numeryczna (odbiornik krotkofalowy, wyswietlacz VFD, oscyloskop)
  E. kamera termowizyjna (pusty pokoj... i 36,6 st. C za drzwiami)
  F. sonar (kontakt, ktory z kazda minuta jest blizej)

  python tools/prerender/horror_options/mockups3.py
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
BAYER4 = np.array([[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]])
DATE = "PIĄTEK 02.10.2026"


def F(name, px):
    return ImageFont.truetype(str(FONTS / name), px)


def to565(a):
    t = np.tile(BAYER4, (H // 4 + 1, W // 4 + 1))[:H, :W][..., None] / 16.0
    s = np.array([8, 4, 8])
    return Image.fromarray(np.clip(np.floor(np.clip(a + t * s, 0, 255) / s) * s, 0, 255).astype(np.uint8))


def M(fn, blur=0):
    m = Image.new("L", (W, H), 0)
    fn(ImageDraw.Draw(m))
    if blur:
        m = m.filter(ImageFilter.GaussianBlur(blur))
    return np.asarray(m, np.float32)[..., None] / 255


def over(a, m, rgb, k=1.0):
    a[:] = a * (1 - m * k) + np.array(rgb, np.float32) * m * k


def glow(a, fn, rgb, core=(255, 255, 255), radii=((5, 0.6), (2, 0.8)), core_k=0.9):
    for r, k in radii:
        a += M(fn, r) * np.array(rgb, np.float32) * k
    a += M(fn) * np.array(core, np.float32) * core_k


def ctext(d, y, t, f, x=None):
    b = f.getbbox(t)
    d.text(((W - (b[2] - b[0])) / 2 - b[0] if x is None else x, y - b[1]), t, font=f, fill=255)


# ================================================================ D. STACJA NUMERYCZNA
def radio(receiving=False, seed=1):
    r = random.Random(seed)
    yy, xx = np.mgrid[0:H, 0:W]
    a = np.zeros((H, W, 3), np.float32)
    # panel odbiornika: ciemny bakelit ze szczotkowaniem
    a[:] = np.array([34, 31, 28])
    a += np.random.default_rng(seed).normal(0, 3, (H, 1, 1)) + np.random.default_rng(seed + 1).normal(0, 2, (H, W, 1))
    # okno VFD
    over(a, M(lambda d: d.rounded_rectangle([18, 12, W - 18, 132], radius=8, fill=255)), (6, 8, 7))
    amber, core = (255, 150, 30), (255, 225, 170)
    glow(a, lambda d: ctext(d, 22, "23:47", F("DSEG7Classic-Bold.ttf", 72)), amber, core)
    # wygaszone segmenty "88:88" - jak w prawdziwym VFD
    a += M(lambda d: ctext(d, 22, "88:88", F("DSEG7Classic-Bold.ttf", 72))) * np.array([22, 12, 4])
    glow(a, lambda d: ctext(d, 104, DATE, F("VT323.ttf", 22)), amber, core, radii=((2, 0.5),), core_k=0.6)
    glow(a, lambda d: d.text((30, 18), "6.840 MHz", font=F("VT323.ttf", 20), fill=255), amber, core, radii=((2, 0.4),), core_k=0.5)
    glow(a, lambda d: d.text((W - 92, 18), "USB  AM", font=F("VT323.ttf", 20), fill=255), amber, core, radii=((2, 0.3),), core_k=0.35)
    if receiving:
        glow(a, lambda d: d.text((W - 190, 42), "47 · 12 · 09 · 33", font=F("VT323.ttf", 22), fill=255),
             (255, 60, 30), (255, 190, 160), radii=((3, 0.6),), core_k=0.9)

    # skala strojenia z czerwona wskazowka
    over(a, M(lambda d: d.rectangle([18, 146, W - 18, 186], fill=255)), (190, 178, 150))
    def scale(d):
        for i in range(0, 61):
            x = 30 + i * (W - 60) / 60
            d.line([(x, 150), (x, 158 if i % 5 else 164)], fill=255)
        for i, lab in enumerate(["3", "4", "5", "6", "7", "8", "9"]):
            d.text((26 + i * (W - 60) / 6, 166), lab, font=F("VT323.ttf", 18), fill=255)
    over(a, M(scale), (40, 34, 28))
    nx = 30 + (W - 60) * (0.64 if receiving else 0.61)
    over(a, M(lambda d: d.line([(nx, 144), (nx, 188)], fill=255, width=2)), (200, 20, 15))

    # miernik sygnalu (S-metr)
    cx, cy = 120, 300
    over(a, M(lambda d: d.rounded_rectangle([24, 198, 216, 306], radius=6, fill=255)), (205, 192, 160))
    def meter(d):
        for i in range(11):
            ang = math.pi * (1.15 + 0.7 * i / 10)
            d.line([(cx + math.cos(ang) * 80, cy + math.sin(ang) * 80), (cx + math.cos(ang) * 90, cy + math.sin(ang) * 90)], fill=255, width=2)
        d.text((40, 206), "S", font=F("VT323.ttf", 20), fill=255)
        d.text((180, 206), "+40", font=F("VT323.ttf", 18), fill=255)
    over(a, M(meter), (40, 34, 30))
    lvl = 0.86 if receiving else 0.18
    ang = math.pi * (1.15 + 0.7 * lvl)
    over(a, M(lambda d: d.line([(cx, cy), (cx + math.cos(ang) * 92, cy + math.sin(ang) * 92)], fill=255, width=2)), (20, 18, 16))

    # oscyloskop
    ox0, oy0, ox1, oy1 = 236, 198, W - 24, 306
    over(a, M(lambda d: d.rounded_rectangle([ox0, oy0, ox1, oy1], radius=6, fill=255)), (4, 12, 6))
    over(a, M(lambda d: [d.line([(x, oy0 + 4), (x, oy1 - 4)], fill=255) for x in range(ox0 + 22, ox1, 22)] +
              [d.line([(ox0 + 4, y), (ox1 - 4, y)], fill=255) for y in range(oy0 + 18, oy1, 18)]), (10, 40, 18))
    pts = []
    for x in range(ox0 + 6, ox1 - 6):
        t = (x - ox0) / 9
        amp = (30 * math.sin(t * 0.9) * math.sin(t * 0.21) + r.gauss(0, 4)) if receiving else r.gauss(0, 3)
        pts.append((x, (oy0 + oy1) / 2 + amp))
    glow(a, lambda d: d.line(pts, fill=255, width=1), (40, 255, 90), (200, 255, 210), radii=((3, 0.5),), core_k=0.8)
    over(a, M(lambda d: d.text((30, 290), "ODBIÓR: STACJA 7", font=F("VT323.ttf", 16), fill=255)), (60, 50, 40))
    a *= (1 - 0.35 * (np.hypot((xx - W / 2) / (W / 1.4), (yy - H / 2) / (H / 1.2))) ** 2)[..., None]
    return to565(a)


# ================================================================ E. TERMOWIZJA
def ironbow(t):
    stops = [(0.0, (0, 0, 10)), (0.2, (30, 0, 90)), (0.4, (120, 0, 140)), (0.6, (210, 40, 60)),
             (0.8, (250, 150, 0)), (0.95, (255, 240, 120)), (1.0, (255, 255, 255))]
    t = np.clip(t, 0, 1)
    out = np.zeros(t.shape + (3,), np.float32)
    for (t0, c0), (t1, c1) in zip(stops, stops[1:]):
        m = (t >= t0) & (t <= t1)
        u = ((t - t0) / (t1 - t0))[m][:, None]
        out[m] = np.array(c0) * (1 - u) + np.array(c1) * u
    return out


def thermal(person=False, seed=2):
    yy, xx = np.mgrid[0:H, 0:W].astype(np.float32)
    temp = np.full((H, W), 0.30, np.float32)                 # pokoj ~18 st.
    temp += (yy / H) * 0.06                                   # podloga cieplejsza
    def rect(x0, y0, x1, y1, v, blur=3):
        m = M(lambda d: d.rectangle([x0, y0, x1, y1], fill=255), blur)[..., 0]
        return m * v
    temp -= rect(300, 40, 430, 150, 0.16, 4)                  # okno (zimne)
    temp -= rect(312, 48, 418, 142, 0.05, 1)
    temp += rect(316, 170, 420, 230, 0.36, 3)                 # kaloryfer
    for x in range(322, 416, 10):
        temp -= rect(x, 172, x + 3, 228, 0.12, 1)
    temp -= rect(40, 60, 150, 300, 0.07, 3)                   # drzwi (uchylone)
    temp -= rect(140, 60, 158, 300, 0.12, 2)                  # szpara drzwi - ciemna
    temp += rect(186, 200, 270, 300, 0.05, 6)                 # fotel
    if person:  # postac zagladajaca zza drzwi: glowa + bark + reka na framudze
        pm = M(lambda d: (d.ellipse([136, 92, 166, 132], fill=255), d.rectangle([145, 128, 157, 140], fill=255),
                          d.polygon([(132, 142), (148, 136), (156, 136), (172, 142), (174, 230), (128, 230)], fill=255),
                          d.polygon([(166, 150), (180, 158), (178, 166), (164, 160)], fill=255)), 5)[..., 0]
        temp = temp * (1 - pm) + 0.93 * pm
        temp += M(lambda d: (d.ellipse([144, 108, 150, 114], fill=255), d.ellipse([154, 108, 160, 114], fill=255)), 1)[..., 0] * 0.06
    temp += np.random.default_rng(seed).normal(0, 0.008, temp.shape)
    a = ironbow(temp)
    # OSD kamery
    def osd(d):
        d.text((14, 8), "TERMOWIZJA", font=F("VT323.ttf", 22), fill=255)
        d.text((14, 290), DATE, font=F("VT323.ttf", 22), fill=255)
        d.text((W - 98, 290), "ε 0.98", font=F("VT323.ttf", 22), fill=255)
        cx, cy = 240, 168
        d.line([(cx - 14, cy), (cx - 4, cy)], fill=255, width=2)
        d.line([(cx + 4, cy), (cx + 14, cy)], fill=255, width=2)
        d.line([(cx, cy - 14), (cx, cy - 4)], fill=255, width=2)
        d.line([(cx, cy + 4), (cx, cy + 14)], fill=255, width=2)
        d.text((cx + 18, cy - 8), "SP1 18.4°C", font=F("VT323.ttf", 20), fill=255)
    a = a * (1 - M(osd, 1.5) * 0.7)
    over(a, M(osd), (240, 240, 240))
    # skala temperatur
    bar = np.linspace(1, 0, 200)[:, None].repeat(12, 1)
    a[60:260, W - 30:W - 18] = ironbow(bar)
    over(a, M(lambda d: (d.text((W - 72, 48), "37.0", font=F("VT323.ttf", 18), fill=255),
                         d.text((W - 72, 256), "12.0", font=F("VT323.ttf", 18), fill=255))), (240, 240, 240))
    if person:
        over(a, M(lambda d: d.rectangle([118, 84, 186, 240], outline=255, width=2)), (255, 255, 255))
        over(a, M(lambda d: d.text((122, 244), "36.6°C", font=F("VT323.ttf", 22), fill=255)), (255, 255, 255))
    # godzina
    f = F("VT323.ttf", 96)
    over(a, M(lambda d: ctext(d, 24, "23:47", f), 3), (0, 0, 0), 0.8)
    over(a, M(lambda d: ctext(d, 24, "23:47", f)), (245, 245, 245))
    return to565(a)


# ================================================================ F. SONAR
def sonar(minute=12, sweep_deg=40, seed=3):
    r = random.Random(seed)
    yy, xx = np.mgrid[0:H, 0:W].astype(np.float32)
    a = np.zeros((H, W, 3), np.float32)
    a[:] = (2, 8, 6)
    cx, cy, R = 240, 194, 120
    dist = np.hypot(xx - cx, yy - cy)
    inside = dist <= R
    a[inside] = (4, 22, 14)
    green, core = (40, 255, 120), (190, 255, 210)
    # pierscienie i linie namiaru
    def grid(d):
        for k in (1, 2, 3, 4):
            rr = R * k / 4
            d.ellipse([cx - rr, cy - rr, cx + rr, cy + rr], outline=255)
        for deg in range(0, 360, 30):
            t = math.radians(deg)
            d.line([(cx, cy), (cx + math.cos(t) * R, cy + math.sin(t) * R)], fill=255)
    a += M(grid) * np.array([20, 80, 45])
    # smuga przemiatania (zanik za wiazka)
    ang = (np.degrees(np.arctan2(yy - cy, xx - cx)) - (sweep_deg - 90)) % 360
    trail = np.where(inside, np.clip(1 - ang / 110, 0, 1) ** 2, 0)
    a += trail[..., None] * np.array([10, 90, 45])
    t = math.radians(sweep_deg - 90)
    glow(a, lambda d: d.line([(cx, cy), (cx + math.cos(t) * R, cy + math.sin(t) * R)], fill=255, width=2), green, core,
         radii=((4, 0.6),), core_k=0.8)
    # szum dna / ryby
    for _ in range(40):
        rr, th = r.uniform(0.3, 1.0) * R, r.uniform(0, 2 * math.pi)
        x, y = cx + math.cos(th) * rr, cy + math.sin(th) * rr
        a += M(lambda d: d.ellipse([x - 1, y - 1, x + 1, y + 1], fill=255), 0.8) * np.array([20, 120, 60]) * r.uniform(0.2, 0.8)
    # kontakt: z kazda minuta blizej srodka
    cd = R * (0.95 - 0.85 * minute / 59)
    th = math.radians(-40)
    bx, by = cx + math.cos(th) * cd, cy + math.sin(th) * cd
    size = 4 + 6 * minute / 59
    glow(a, lambda d: d.ellipse([bx - size, by - size * 0.7, bx + size, by + size * 0.7], fill=255),
         (255, 200, 60), (255, 240, 200), radii=((5, 0.7), (2, 0.9)), core_k=1.0)
    m_ = int(312 * (1 - minute / 59)) + 12
    glow(a, lambda d: d.text((bx + 12, by - 24), f"KONTAKT 1  {m_} m", font=F("VT323.ttf", 18), fill=255),
         (255, 200, 60), (255, 230, 180), radii=((2, 0.4),), core_k=0.85)
    # odczyty
    def osd(d):
        d.text((14, 290), DATE, font=F("VT323.ttf", 20), fill=255)
        d.text((W - 140, 290), "GŁĘB. 1840 m", font=F("VT323.ttf", 20), fill=255)
        d.text((14, 74), "ZAKRES", font=F("VT323.ttf", 18), fill=255)
        d.text((14, 92), "400 m", font=F("VT323.ttf", 18), fill=255)
        d.text((W - 92, 74), "NAMIAR", font=F("VT323.ttf", 18), fill=255)
        d.text((W - 92, 92), "040°", font=F("VT323.ttf", 18), fill=255)
    glow(a, osd, green, core, radii=((2, 0.3),), core_k=0.6)
    glow(a, lambda d: ctext(d, 6, "23:47", F("DSEG7Classic-Bold.ttf", 52)), green, core, radii=((5, 0.5), (2, 0.7)), core_k=0.9)
    a *= (1 - 0.3 * (np.hypot((xx - W / 2) / (W / 1.3), (yy - H / 2) / (H / 1.2))) ** 2)[..., None]
    return to565(a)


def strip(frames):
    s = Image.new("RGB", (W * len(frames) + 8 * (len(frames) - 1), H), (0, 0, 0))
    for i, f in enumerate(frames):
        s.paste(f, (i * (W + 8), 0))
    return s


def main():
    OUT.mkdir(exist_ok=True)
    strip([radio(), radio(receiving=True)]).save(OUT / "D_stacja.png")
    strip([thermal(), thermal(person=True)]).save(OUT / "E_termowizja.png")
    strip([sonar(12, 40), sonar(52, 200)]).save(OUT / "F_sonar.png")
    print("zapisano", OUT)


if __name__ == "__main__":
    main()
