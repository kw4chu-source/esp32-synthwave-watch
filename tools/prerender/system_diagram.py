"""Schemat calego systemu do README (neonowy styl): docs/img/system.png

  python tools/prerender/system_diagram.py
"""

from __future__ import annotations

import math
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont

HERE = Path(__file__).parent
OUT = HERE.parents[1] / "docs" / "img" / "system.png"
FONTS = HERE / "fonts"
W, H = 1240, 600
BG = (13, 10, 24)
PINK, CYAN, YELLOW, PURPLE, GREEN = (255, 60, 200), (0, 225, 255), (252, 230, 60), (170, 120, 255), (90, 255, 150)


def F(size, name="Rajdhani-Bold.ttf"):
    return ImageFont.truetype(str(FONTS / name), size)


class Canvas:
    def __init__(self):
        self.base = Image.new("RGB", (W, H), BG)
        self.glow = Image.new("RGB", (W, H), (0, 0, 0))
        self.d = ImageDraw.Draw(self.base)
        self.g = ImageDraw.Draw(self.glow)

    def box(self, xy, color, title, lines=(), fill=(20, 16, 38)):
        x0, y0, x1, y1 = xy
        self.d.rounded_rectangle(xy, radius=12, fill=fill, outline=color, width=2)
        self.g.rounded_rectangle(xy, radius=12, outline=color, width=4)
        cx = (x0 + x1) // 2
        self.d.text((cx, y0 + 22), title, font=F(24), fill=(255, 255, 255), anchor="mm")
        for i, ln in enumerate(lines):
            self.d.text((cx, y0 + 50 + i * 20), ln, font=F(17, "Rajdhani-Bold.ttf"), fill=(190, 175, 220), anchor="mm")

    def arrow(self, pts, color, label=None, label_at=None, dashed=False, both=False):
        segs = list(zip(pts, pts[1:]))
        for (ax, ay), (bx, by) in segs:
            if dashed:
                n = int(math.hypot(bx - ax, by - ay) // 14)
                for k in range(n):
                    if k % 2 == 0:
                        t0, t1 = k / n, (k + 1) / n
                        p = [(ax + (bx - ax) * t0, ay + (by - ay) * t0), (ax + (bx - ax) * t1, ay + (by - ay) * t1)]
                        self.d.line(p, fill=color, width=2)
                        self.g.line(p, fill=color, width=4)
            else:
                self.d.line([(ax, ay), (bx, by)], fill=color, width=2)
                self.g.line([(ax, ay), (bx, by)], fill=color, width=5)
        def head(a, b):
            ang = math.atan2(b[1] - a[1], b[0] - a[0])
            tip = b
            l = [(tip[0] - 14 * math.cos(ang - 0.4), tip[1] - 14 * math.sin(ang - 0.4)),
                 (tip[0] - 14 * math.cos(ang + 0.4), tip[1] - 14 * math.sin(ang + 0.4))]
            self.d.polygon([tip] + l, fill=color)
            self.g.polygon([tip] + l, fill=color)
        head(pts[-2], pts[-1])
        if both:
            head(pts[1], pts[0])
        if label:
            x, y = label_at
            f = F(17)
            tw = self.d.textlength(label, font=f)
            self.d.rounded_rectangle([x - tw / 2 - 6, y - 11, x + tw / 2 + 6, y + 11], radius=6, fill=BG)
            self.d.text((x, y), label, font=f, fill=color, anchor="mm")

    def finish(self):
        g = np.asarray(self.glow.filter(ImageFilter.GaussianBlur(7)), np.float32)
        b = np.asarray(self.base, np.float32)
        return Image.fromarray(np.clip(b + g * 0.9, 0, 255).astype(np.uint8))


def main():
    c = Canvas()
    # strefa "Dom"
    c.d.rounded_rectangle([24, 118, 900, 576], radius=18, outline=(70, 55, 110), width=2)
    c.d.text((44, 136), "DOM", font=F(22), fill=(120, 100, 170), anchor="lm")

    c.box((330, 20, 520, 86), YELLOW, "OpenWeather", ["pogoda co 15 min"])
    c.box((560, 20, 750, 86), YELLOW, "pool.ntp.org", ["czas co 6 h"])
    c.box((60, 170, 260, 250), PURPLE, "Router domowy", ["jedyny klient: bramka"])
    c.box((330, 160, 600, 290), CYAN, "Bramka ESP-NET",
          ["ESP32 + antena, NAT", "kaganiec ruchu", "NTP + pogoda (UDP 4210)"])
    c.box((680, 250, 870, 380), PINK, "Zegarek", ["ESP32 + ILI9488", "tarcze z pogodą", "loader w factory"])
    c.box((330, 420, 600, 540), GREEN, "Cardputer ADV", ["hub: Flasher, Upload", "karta SD z tarczami"])
    c.box((60, 420, 260, 500), PURPLE, "Laptop", ["PlatformIO, push.py"])
    c.box((960, 410, 1200, 520), YELLOW, "VPS", ["WireGuard", "(Docker)"])

    c.arrow([(425, 86), (440, 160)], YELLOW)
    c.arrow([(655, 86), (560, 160)], YELLOW)
    c.arrow([(260, 210), (330, 210)], PURPLE, both=True)
    c.arrow([(600, 260), (680, 300)], CYAN, "czas + pogoda", (622, 312))
    c.arrow([(465, 290), (465, 420)], CYAN, "ESP-NET", (465, 352), both=True)
    c.arrow([(600, 515), (775, 515), (775, 380)], GREEN, "ESP-NOW + AP: wgrywanie", (688, 515))
    c.arrow([(160, 420), (160, 330), (380, 330), (380, 290)], PURPLE, "upload .bin", (250, 330))
    c.arrow([(600, 445), (930, 445), (960, 445)], GREEN, "poza domem: hotspot + WireGuard", (850, 422), dashed=True)
    c.arrow([(160, 500), (160, 560), (1080, 560), (1080, 520)], PURPLE, "SSH", (930, 560), dashed=True)
    OUT.parent.mkdir(parents=True, exist_ok=True)
    c.finish().save(OUT, optimize=True)
    print(OUT)


if __name__ == "__main__":
    main()
