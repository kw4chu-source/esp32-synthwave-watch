"""Zasoby tarczy "neon_rain" - neonowe miasto w deszczu.

Statyczne tlo (niebo, miasto, szyldy, tablica zegara, mokra ulica z
odbiciami) liczone tutaj raz i zapisane jako RGB565 we flashu. Na ESP32
zostaja tylko warstwy ruchome: cyfry-neony, data, deszcz, okna, auto,
hologram, falowanie odbic.

Wynik:
  src/faces/neon_rain/assets_gen.h / .cpp
  tools/prerender/neon_rain/out/preview.png (+ _x2) - podglad klatki

  python tools/prerender/neon_rain/gen_assets.py
"""

from __future__ import annotations

import math
import random
import urllib.request
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont

HERE = Path(__file__).parent
ROOT = HERE.parents[2]
FONTS = HERE.parent / "fonts"
OUT_SRC = ROOT / "src" / "faces" / "neon_rain"
OUT_PREVIEW = HERE / "out"

W, H = 480, 320
STREET_Y = 286

SIGN_BOX = (58, 34, 422, 176)
DIGIT_H = 92
TUBE_RGB = (255, 70, 180)
TUBE_CORE = (255, 215, 240)
FRAME_RGB = (0, 225, 255)
DATE_RGB = (255, 200, 40)
DATE_CORE = (255, 240, 190)
DATE_PX = 19
GLOW_PAD = 14          # margines komorki cyfry na poswiate
DATE_PAD = 4

JP_RGB = (255, 90, 30)
HOLO_BOX = (430, 52, 474, 168)
HOLO_RGB = (40, 240, 255)

WEEKDAYS = ["NIEDZIELA", "PONIEDZIAŁEK", "WTOREK", "ŚRODA", "CZWARTEK", "PIĄTEK", "SOBOTA"]
DATE_CHARSET = sorted(set("".join(WEEKDAYS) + "0123456789. "))

FLICKER_WINDOWS = 48

NOTO_JP_URL = "https://github.com/google/fonts/raw/main/ofl/notosansjp/NotoSansJP%5Bwght%5D.ttf"


# ---------------------------------------------------------------- pomocnicze
def font(name, px):
    path = FONTS / name
    if not path.exists() and name == "NotoSansJP.ttf":  # 9,5 MB - poza repo
        print("pobieram NotoSansJP.ttf ...")
        urllib.request.urlretrieve(NOTO_JP_URL, path)
    return ImageFont.truetype(str(path), px)


def rgb565(r, g, b):
    return ((int(r) & 0xF8) << 8) | ((int(g) & 0xFC) << 3) | (int(b) >> 3)


def mask_np(img):
    return np.asarray(img, np.float32) / 255.0


def glow_intensity(mask, radii):
    """Suma rozmyc maski (poswiata neonu), 0..~1+."""
    pil = Image.fromarray((np.clip(mask, 0, 1) * 255).astype(np.uint8))
    acc = np.zeros(mask.shape, np.float32)
    for r, k in radii:
        acc += mask_np(pil.filter(ImageFilter.GaussianBlur(r))) * k * 1.6
    return acc


def add_glow(img, mask, rgb, core_rgb=None, radii=((10, 0.55), (4, 0.9)), core=1.0):
    img += glow_intensity(mask, radii)[..., None] * np.array(rgb, np.float32)
    core_rgb = core_rgb or tuple(min(255, c * 0.45 + 150) for c in rgb)
    img += mask[..., None] * np.array(core_rgb, np.float32) * core


BAYER4 = np.array([[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]])


def dither565(img):
    h, w, _ = img.shape
    t = np.tile(BAYER4, (h // 4 + 1, w // 4 + 1))[:h, :w][..., None] / 16.0
    steps = np.array([8, 4, 8])
    return np.clip(np.floor(np.clip(img + t * steps, 0, 255) / steps) * steps, 0, 255)


# ---------------------------------------------------------------- tlo
def sky(img):
    for y in range(H):
        t = y / STREET_Y
        c = np.array([4, 3, 16]) * (1 - t) + np.array([46, 10, 62]) * t
        if y > 150:
            c += np.array([40, 6, 40]) * ((y - 150) / (STREET_Y - 150)) ** 2
        img[y] = c


def buildings(img, top_range, w_range, color, win_colors, density, seed, windows_out=None):
    r = random.Random(seed)
    x = -10
    while x < W:
        bw = r.randint(*w_range)
        top = r.randint(*top_range)
        img[top:STREET_Y, max(0, x):min(W, x + bw)] = color
        if r.random() < 0.25 and 0 <= x + bw // 2 < W:
            img[top - r.randint(6, 18):top, x + bw // 2] = color
        for wy in range(top + 6, STREET_Y - 6, 7):
            for wx in range(x + 4, x + bw - 4, 6):
                if 0 <= wx < W - 3 and r.random() < density:
                    c = r.choice(win_colors)
                    img[wy:wy + 4, wx:wx + 3] = c
                    if windows_out is not None:
                        windows_out.append((wx, wy, c, color))
        x += bw + r.randint(0, 3)


def sign_tablet(img):
    x0, y0, x1, y1 = SIGN_BOX
    img[y0:y1, x0:x1] = img[y0:y1, x0:x1] * 0.25 + np.array([10, 3, 18]) * 0.75
    frame = Image.new("L", (W, H), 0)
    ImageDraw.Draw(frame).rounded_rectangle([x0 + 4, y0 + 4, x1 - 4, y1 - 4], radius=14,
                                            outline=255, width=2)
    add_glow(img, mask_np(frame), FRAME_RGB, radii=((8, 0.45), (3, 0.8)), core=0.9)


def jp_sign(img):
    m = Image.new("L", (W, H), 0)
    d = ImageDraw.Draw(m)
    d.rectangle([14, 44, 44, 172], outline=255, width=1)
    f = font("NotoSansJP.ttf", 22)
    for i, ch in enumerate("ネオン街"):
        d.text((18, 48 + i * 30), ch, font=f, fill=255)
    add_glow(img, mask_np(m), JP_RGB, radii=((6, 0.5), (2, 0.7)), core=0.9)


def hologram(img):
    x0, y0, x1, y1 = HOLO_BOX
    panel = np.zeros((y1 - y0, x1 - x0, 3), np.float32)
    panel[:] = (0, 60, 70)
    panel[::3] = (0, 110, 120)
    img[y0:y1, x0:x1] = img[y0:y1, x0:x1] * 0.55 + panel * 0.6
    m = Image.new("L", (W, H), 0)
    d = ImageDraw.Draw(m)
    d.text((x0 + 4, y0 + 18), "愛", font=font("NotoSansJP.ttf", 36), fill=255)
    d.text((x0 + 6, y0 + 74), "AI-CO", font=font("ShareTechMono.ttf", 11), fill=200)
    d.rectangle([x0, y0, x1 - 1, y1 - 1], outline=160)
    add_glow(img, mask_np(m), HOLO_RGB, radii=((5, 0.4),), core=0.8)


def street(img, rnd):
    src = img[STREET_Y - (H - STREET_Y) * 2:STREET_Y][::-1]
    refl = np.stack([src[min(len(src) - 1, i * 2)] for i in range(H - STREET_Y)])
    refl = np.asarray(Image.fromarray(np.clip(refl, 0, 255).astype(np.uint8))
                      .filter(ImageFilter.GaussianBlur(1.6)), np.float32)
    fade = np.linspace(0.42, 0.18, H - STREET_Y)[:, None, None]
    img[STREET_Y:] = np.array([5, 3, 10]) + refl * fade
    img[STREET_Y] = img[STREET_Y] * 0.6 + np.array([90, 40, 110]) * 0.4
    streaks = [(SIGN_BOX[0] + 20, SIGN_BOX[2] - 20, TUBE_RGB, 0.55),
               (SIGN_BOX[0], SIGN_BOX[2], FRAME_RGB, 0.25),
               (14, 44, JP_RGB, 0.5), (430, 474, HOLO_RGB, 0.45)]
    for xa, xb, rgb, k in streaks:
        m = np.zeros((H - STREET_Y, W), np.float32)
        for x in range(xa, xb):
            if rnd.random() < 0.55:
                ln = rnd.randint(8, H - STREET_Y)
                m[:ln, x] = np.linspace(1, 0, ln) * rnd.uniform(0.4, 1)
        m = mask_np(Image.fromarray((m * 255).astype(np.uint8)).filter(ImageFilter.GaussianBlur(1.2)))
        img[STREET_Y + 1:] += (m[..., None] * np.array(rgb, np.float32) * k)[:H - STREET_Y - 1]


def build_background():
    rnd = random.Random(2077)
    img = np.zeros((H, W, 3), np.float32)
    sky(img)
    buildings(img, (110, 175), (22, 48), (16, 11, 34),
              [(42, 42, 84), (63, 35, 77), (28, 63, 84)], 0.18, seed=1)
    windows = []
    buildings(img, (170, 235), (30, 70), (7, 5, 16),
              [(230, 190, 100), (255, 120, 60), (60, 220, 255), (255, 80, 180)], 0.12, seed=2,
              windows_out=windows)
    jp_sign(img)
    hologram(img)
    sign_tablet(img)
    street(img, rnd)
    rnd.shuffle(windows)
    return dither565(img), windows[:FLICKER_WINDOWS]


# ---------------------------------------------------------------- neony (cyfry, data)
def q4(a):
    return np.clip(np.round(a * 15), 0, 15).astype(np.uint8)


# Glow kodowany nieliniowo (pierwiastek): wiecej poziomow w slabych ogonach
# poswiaty, bez widocznych schodkow. Dekodowanie: I = (g/15)^2 * imax.
def neon_glyph(fnt, ch, cell_w, cell_h, x_off, y_off, radii):
    """Komorka: index = (core4 << 4) | glow4."""
    m = Image.new("L", (cell_w, cell_h), 0)
    ImageDraw.Draw(m).text((x_off, y_off), ch, font=fnt, fill=255)
    core = mask_np(m)
    imax = sum(k for _, k in radii) * 1.6
    glow = np.clip(glow_intensity(core, radii) / imax, 0, 1)
    return (q4(core) << 4) | q4(np.sqrt(glow))


def neon_lut(tube, core_rgb, radii, glow_scale=1.0, core_scale=1.0):
    """256 wpisow RGB888 dodawanych do tla (neon = swiatlo, dodawanie z
    nasyceniem). Na ESP32 dodawane z ditheringiem Bayera."""
    imax = sum(k for _, k in radii) * 1.6
    out = []
    for idx in range(256):
        c = (idx >> 4) / 15.0 * core_scale
        i = ((idx & 15) / 15.0) ** 2 * imax * glow_scale
        out.append(tuple(int(min(255, core_rgb[k] * c + tube[k] * i)) for k in range(3)))
    return out


DIGIT_RADII = ((10, 0.55), (4, 0.9))
DATE_RADII = ((3, 0.5),)


def build_digits():
    f = font("TiltNeon.ttf", 100)
    b0 = f.getbbox("0")
    px = round(100 * DIGIT_H / (b0[3] - b0[1]))
    f = font("TiltNeon.ttf", px)
    b0 = f.getbbox("0")
    ink_w = max(f.getbbox(c)[2] - f.getbbox(c)[0] for c in "0123456789")
    cell_w, cell_h = ink_w + 2 * GLOW_PAD, (b0[3] - b0[1]) + 2 * GLOW_PAD
    radii = DIGIT_RADII
    digits = []
    for c in "0123456789":
        bb = f.getbbox(c)
        x_off = (cell_w - (bb[2] - bb[0])) // 2 - bb[0]
        digits.append(neon_glyph(f, c, cell_w, cell_h, x_off, GLOW_PAD - b0[1], radii))
    cb = f.getbbox(":")
    colon_w = (cb[2] - cb[0]) + 2 * GLOW_PAD
    colon = neon_glyph(f, ":", colon_w, cell_h, GLOW_PAD - cb[0], GLOW_PAD - b0[1], radii)

    # uklad jak w makiecie: HH:MM wysrodkowane na tablicy, proporcjonalne odstepy
    x0, y0, x1, y1 = SIGN_BOX
    adv = f.getlength("0")
    colon_adv = f.getlength(":")
    total = 4 * adv + colon_adv
    tx = (x0 + x1 - total) / 2
    top = round((y0 + y1) / 2 - 12 - (b0[3] - b0[1]) / 2) - GLOW_PAD
    xs, x = [], tx
    for i in range(5):
        if i == 2:
            colon_x = round(x + (colon_adv - (cb[2] - cb[0])) / 2) - GLOW_PAD
            x += colon_adv
            continue
        xs.append(round(x + (adv - ink_w) / 2) - GLOW_PAD)
        x += adv
    return dict(px=px, cell_w=cell_w, cell_h=cell_h, digits=digits, colon=colon,
                colon_w=colon_w, xs=xs, colon_x=colon_x, top=top)


def build_date_atlas():
    f = font("TiltNeon.ttf", DATE_PX)
    ascent, _ = f.getmetrics()
    glyphs = []
    for ch in DATE_CHARSET:
        adv = int(round(f.getlength(ch)))
        bb = f.getbbox(ch)
        if ch == " " or bb[2] <= bb[0]:
            glyphs.append(dict(cp=ord(ch), w=0, h=0, xoff=0, yoff=0, adv=adv, data=np.zeros((0,), np.uint8)))
            continue
        w, h = bb[2] - bb[0] + 2 * DATE_PAD, bb[3] - bb[1] + 2 * DATE_PAD
        data = neon_glyph(f, ch, w, h, DATE_PAD - bb[0], DATE_PAD - bb[1], DATE_RADII)
        glyphs.append(dict(cp=ord(ch), w=w, h=h, xoff=bb[0] - DATE_PAD, yoff=bb[1] - ascent - DATE_PAD,
                           adv=adv, data=data))
    return glyphs, ascent


# ---------------------------------------------------------------- auto
def build_car():
    cw, ch = 64, 22
    img = np.zeros((ch, cw, 3), np.float32)
    alpha = np.zeros((ch, cw), np.float32)
    cx, cy = 32, 11
    body = Image.new("L", (cw, ch), 0)
    ImageDraw.Draw(body).polygon([(cx - 26, cy + 4), (cx - 15, cy - 4), (cx + 12, cy - 5),
                                  (cx + 27, cy + 2), (cx + 20, cy + 7), (cx - 20, cy + 7)], fill=255)
    b = mask_np(body)
    img += b[..., None] * np.array([12, 10, 22])
    alpha = np.maximum(alpha, b)
    for pts, rgb in (([(cx + 24, cy + 1), (cx + 26, cy + 2)], (255, 255, 230)),
                     ([(cx - 24, cy + 3), (cx - 22, cy + 4)], (255, 30, 30))):
        m = Image.new("L", (cw, ch), 0)
        ImageDraw.Draw(m).line(pts, fill=255)
        mm = mask_np(m)
        g = np.clip(glow_intensity(mm, ((4, 0.9),)) + mm, 0, 1)
        img = img * (1 - g[..., None]) + np.array(rgb, np.float32) * g[..., None]
        alpha = np.maximum(alpha, g)
    q = dither565(img)
    colors = [rgb565(*q[y, x]) for y in range(ch) for x in range(cw)]
    return cw, ch, colors, (np.clip(alpha, 0, 1) * 255).astype(np.uint8).flatten()


# ---------------------------------------------------------------- podglad
def preview(bg, dg, lut_on, date_glyphs, date_base, rnd, text="23:47", date="PIĄTEK  02.10.2026"):
    img = bg.astype(np.float32).copy()

    def blit(data, w, h, gx, gy, lut):
        for yy in range(h):
            for xx in range(w):
                idx = data[yy * w + xx] if data.ndim == 1 else data[yy, xx]
                if idx and 0 <= gy + yy < H and 0 <= gx + xx < W:
                    img[gy + yy, gx + xx] = np.minimum(255, img[gy + yy, gx + xx] + lut[idx])

    for i, ch in enumerate(text.replace(":", "")):
        blit(dg["digits"][int(ch)], dg["cell_w"], dg["cell_h"], dg["xs"][i], dg["top"], lut_on)
    blit(dg["colon"], dg["colon_w"], dg["cell_h"], dg["colon_x"], dg["top"], lut_on)

    lut_date = neon_lut(DATE_RGB, DATE_CORE, DATE_RADII)
    by_cp = {g["cp"]: g for g in date_glyphs}
    tw = sum(by_cp[ord(c)]["adv"] for c in date)
    x = (SIGN_BOX[0] + SIGN_BOX[2] - tw) // 2
    for c in date:
        g = by_cp[ord(c)]
        if g["w"]:
            blit(g["data"], g["w"], g["h"], x + g["xoff"], date_base + g["yoff"], lut_date)
        x += g["adv"]

    for _ in range(70):  # deszcz jak na urzadzeniu: 1 px, nachylenie -0,22
        x0, y0, ln = rnd.uniform(0, W + 30), rnd.uniform(-10, STREET_Y), rnd.randint(9, 17)
        for k in range(ln):
            px, py = int(x0 - k * 0.22), int(y0) + k
            if 0 <= px < W and 0 <= py < H:
                img[py, px] += (np.array([170, 195, 230]) - img[py, px]) * 0.55
    return Image.fromarray(dither565(img).astype(np.uint8))


# ---------------------------------------------------------------- eksport C++
def c_array(name, data, ctype="uint8_t", per_line=32, dims=None):
    vals = [str(int(v)) for v in data]
    lines = [", ".join(vals[i:i + per_line]) for i in range(0, len(vals), per_line)]
    dim = dims or f"[{len(vals)}]"
    return f"const {ctype} {name}{dim} = {{\n  " + ",\n  ".join(lines) + "\n};\n"


def write_sources(bg, windows, dg, lut_on, lut_dim, date_glyphs, date_base, lut_date, car):
    OUT_SRC.mkdir(parents=True, exist_ok=True)
    cw, chh, car_c, car_a = car
    x0, y0, x1, y1 = SIGN_BOX
    h = f"""#pragma once
// WYGENEROWANE przez tools/prerender/neon_rain/gen_assets.py - nie edytowac recznie.
#include <stdint.h>

namespace assets {{

constexpr int STREET_Y = {STREET_Y};

// ---- tlo (RGB565, natywna kolejnosc bajtow) ----
extern const uint16_t BG[{H} * {W}];

// ---- cyfry-neony: index = (core4 << 4) | glow4 (glow nieliniowo) -> LUT ----
constexpr int DIGIT_CELL_W = {dg['cell_w']};
constexpr int DIGIT_CELL_H = {dg['cell_h']};
constexpr int DIGIT_TOP = {dg['top']};
constexpr int DIGIT_X[4] = {{{', '.join(map(str, dg['xs']))}}};
constexpr int COLON_X = {dg['colon_x']};
constexpr int COLON_W = {dg['colon_w']};
extern const uint8_t DIGIT_DATA[10][DIGIT_CELL_W * DIGIT_CELL_H];
extern const uint8_t COLON_DATA[COLON_W * DIGIT_CELL_H];
// LUT: RGB888 dodawane do tla z nasyceniem (neon = swiatlo)
extern const uint8_t LUT_ON[256][3];
extern const uint8_t LUT_DIM[256][3];  // przygaszona rurka (migotanie)

// ---- data (Tilt Neon {DATE_PX}px, ten sam format co cyfry) ----
struct DateGlyph {{ uint16_t cp; uint8_t w, h; int8_t xoff, yoff; uint8_t adv; uint32_t offset; }};
constexpr int DATE_GLYPH_COUNT = {len(date_glyphs)};
constexpr int DATE_BASELINE = {date_base};
constexpr int DATE_CENTER_X = {(x0 + x1) // 2};
constexpr int DATE_BAND_TOP = {date_base - 26};
constexpr int DATE_BAND_BOTTOM = {date_base + 10};
extern const DateGlyph DATE_GLYPHS[DATE_GLYPH_COUNT];
extern const uint8_t DATE_DATA[];
extern const uint8_t LUT_DATE[256][3];
extern const char* const WEEKDAYS[7];

// ---- okna, ktore gasna/zapalaja sie ----
struct Window {{ uint16_t x, y; uint16_t lit, dark; }};
constexpr int WINDOW_COUNT = {len(windows)};
constexpr int WINDOW_W = 3, WINDOW_H = 4;
extern const Window WINDOWS[WINDOW_COUNT];

// ---- latajace auto ----
constexpr int CAR_W = {cw}, CAR_H = {chh};
constexpr int CAR_Y = 7;
extern const uint16_t CAR_COLOR[CAR_W * CAR_H];
extern const uint8_t CAR_ALPHA[CAR_W * CAR_H];

// ---- hologram ----
constexpr int HOLO_X0 = {HOLO_BOX[0]}, HOLO_Y0 = {HOLO_BOX[1]}, HOLO_X1 = {HOLO_BOX[2]}, HOLO_Y1 = {HOLO_BOX[3]};

}}  // namespace assets
"""
    cpp = ["// WYGENEROWANE przez tools/prerender/neon_rain/gen_assets.py - nie edytowac recznie.",
           '#include "assets_gen.h"', "", "namespace assets {", ""]
    bg565 = [rgb565(*bg[y, x]) for y in range(H) for x in range(W)]
    cpp.append(c_array("BG", bg565, "uint16_t", 24, f"[{H} * {W}]"))
    cpp.append("const uint8_t DIGIT_DATA[10][DIGIT_CELL_W * DIGIT_CELL_H] = {")
    for d in dg["digits"]:
        cpp.append("  {" + ",".join(map(str, d.flatten())) + "},")
    cpp.append("};\n")
    cpp.append(c_array("COLON_DATA", dg["colon"].flatten()))
    for name, lut in (("ON", lut_on), ("DIM", lut_dim), ("DATE", lut_date)):
        cpp.append(c_array(f"LUT_{name}", [v for e in lut for v in e], dims="[256][3]"))
    entries, data, off = [], [], 0
    for g in date_glyphs:
        entries.append(f"  {{{g['cp']}, {g['w']}, {g['h']}, {g['xoff']}, {g['yoff']}, {g['adv']}, {off}}},")
        flat = g["data"].flatten()
        data.extend(flat)
        off += len(flat)
    cpp.append("const DateGlyph DATE_GLYPHS[DATE_GLYPH_COUNT] = {\n" + "\n".join(entries) + "\n};\n")
    cpp.append(c_array("DATE_DATA", data, dims="[]"))
    cpp.append("const char* const WEEKDAYS[7] = {" + ", ".join(f'"{d}"' for d in WEEKDAYS) + "};\n")
    cpp.append("const Window WINDOWS[WINDOW_COUNT] = {\n" + "\n".join(
        f"  {{{x}, {y}, {rgb565(*lit)}, {rgb565(*dark)}}}," for x, y, lit, dark in windows) + "\n};\n")
    cpp.append(c_array("CAR_COLOR", car_c, "uint16_t", 16))
    cpp.append(c_array("CAR_ALPHA", car_a))
    cpp.append("}  // namespace assets")
    (OUT_SRC / "assets_gen.h").write_text(h, encoding="utf-8")
    (OUT_SRC / "assets_gen.cpp").write_text("\n".join(cpp) + "\n", encoding="utf-8")
    return (len(bg565) * 2 + sum(d.size for d in dg["digits"]) + dg["colon"].size + len(data)
            + len(car_c) * 3)


def main():
    bg, windows = build_background()
    dg = build_digits()
    lut_on = neon_lut(TUBE_RGB, TUBE_CORE, DIGIT_RADII)
    lut_dim = neon_lut(TUBE_RGB, (120, 40, 90), DIGIT_RADII, glow_scale=0.12, core_scale=0.6)
    date_glyphs, ascent = build_date_atlas()
    date_base = SIGN_BOX[3] - 26 + ascent
    lut_date = neon_lut(DATE_RGB, DATE_CORE, DATE_RADII)
    car = build_car()

    nbytes = write_sources(bg, windows, dg, lut_on, lut_dim, date_glyphs, date_base, lut_date, car)

    OUT_PREVIEW.mkdir(exist_ok=True)
    p = preview(bg, dg, lut_on, date_glyphs, date_base, random.Random(7))
    p.save(OUT_PREVIEW / "preview.png")
    p.resize((W * 2, H * 2), Image.NEAREST).save(OUT_PREVIEW / "preview_x2.png")
    print(f"Tilt Neon {dg['px']}px, komorka cyfry {dg['cell_w']}x{dg['cell_h']}, "
          f"cyfry x={dg['xs']}, dwukropek x={dg['colon_x']}, top={dg['top']}")
    print(f"okna migajace: {len(windows)}, zasoby: {nbytes / 1024:.0f} KB we flashu")


if __name__ == "__main__":
    main()
