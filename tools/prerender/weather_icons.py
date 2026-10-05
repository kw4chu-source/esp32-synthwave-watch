"""Ikony pogody rysowane liniami (maska do neonu) - wspolne dla generatorow tarcz.

Kolejnosc = indeks ikony na urzadzeniu (enum WxIcon w tarczach):
  0 slonce, 1 noc (ksiezyc), 2 chmury, 3 deszcz, 4 snieg, 5 burza, 6 mgla
"""

from __future__ import annotations

import math

import numpy as np
from PIL import Image, ImageDraw, ImageFilter

ICON_NAMES = ["slonce", "noc", "chmury", "deszcz", "snieg", "burza", "mgla"]


def icon(d: ImageDraw.ImageDraw, kind: str, cx: float, cy: float, s: float, width: int = 3):
    """Ikona na masce L (fill 255). s ~ polowa rozmiaru."""
    def line(pts, w=width):
        d.line(pts, fill=255, width=w)

    def cloud(ox, oy, k=1.0):
        r = s * 0.42 * k
        d.arc([ox - r * 2.1, oy - r * 0.9, ox - r * 0.1, oy + r * 1.1], 90, 270, fill=255, width=width)
        d.arc([ox - r * 1.3, oy - r * 1.9, ox + r * 0.7, oy + r * 0.1], 180, 330, fill=255, width=width)
        d.arc([ox - r * 0.1, oy - r * 1.4, ox + r * 1.9, oy + r * 0.6], 230, 90, fill=255, width=width)
        line([ox - r * 1.1, oy + r * 1.1, ox + r * 0.9, oy + r * 1.1])

    if kind == "slonce":
        r = s * 0.45
        d.ellipse([cx - r, cy - r, cx + r, cy + r], outline=255, width=width)
        for a in range(8):
            t = a * math.pi / 4
            line([cx + math.cos(t) * r * 1.45, cy + math.sin(t) * r * 1.45,
                  cx + math.cos(t) * r * 1.95, cy + math.sin(t) * r * 1.95])
    elif kind == "noc":
        r = s * 0.7
        d.arc([cx - r, cy - r, cx + r, cy + r], 70, 290, fill=255, width=width)
        r2 = r * 0.78
        d.arc([cx - r2 + r * 0.42, cy - r2, cx + r2 + r * 0.42, cy + r2], 105, 255, fill=255, width=width)
    elif kind == "chmury":
        cloud(cx, cy)
    elif kind == "deszcz":
        cloud(cx, cy - s * 0.25)
        for i in range(3):
            x = cx - s * 0.45 + i * s * 0.45
            line([x, cy + s * 0.45, x - s * 0.15, cy + s * 0.85])
    elif kind == "snieg":
        cloud(cx, cy - s * 0.25)
        for i in range(3):
            x, y = cx - s * 0.45 + i * s * 0.45, cy + s * 0.65
            for a in range(3):
                t = a * math.pi / 3
                line([x - math.cos(t) * s * 0.13, y - math.sin(t) * s * 0.13,
                      x + math.cos(t) * s * 0.13, y + math.sin(t) * s * 0.13], max(1, width - 1))
    elif kind == "burza":
        cloud(cx, cy - s * 0.25)
        line([cx + s * 0.1, cy + s * 0.3, cx - s * 0.15, cy + s * 0.65, cx + s * 0.1, cy + s * 0.65,
              cx - s * 0.15, cy + s * 1.0])
    elif kind == "mgla":
        for i, (a, b) in enumerate([(-0.8, 0.6), (-0.6, 0.8), (-0.8, 0.5), (-0.5, 0.8)]):
            y = cy - s * 0.6 + i * s * 0.4
            line([cx + a * s, y, cx + b * s, y])


def icon_mask(kind: str, w: int, h: int, s: float, width: int) -> np.ndarray:
    m = Image.new("L", (w, h), 0)
    icon(ImageDraw.Draw(m), kind, w / 2, h / 2, s, width)
    return np.asarray(m, np.float32) / 255
