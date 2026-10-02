"""Zasoby tarczy "night_window" - widok przez okno w deszczowa noc.

Pod latarnia stoi postac; po kazdym mrugnieciu latarni jest blizej okna
(klatka = minuta: 0 pod latarnia ... 59 tuz za szyba). Godzina to odbicie
czerwonego budzika (DSEG7) w szybie.

Tlo (niebo, domy, ulica, plot, ogrodek oswietlony z pokoju) z latarnia
wlaczona + fragment przy zgaszonej. Na ESP32: postac, deszcz za szyba,
krople na szybie (stale i splywajace), odbicie budzika.

  python tools/prerender/night_window/gen_assets.py
"""

from __future__ import annotations

import math
import random
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont

HERE = Path(__file__).parent
ROOT = HERE.parents[2]
FONTS = HERE.parent / "fonts"
OUT_SRC = ROOT / "src" / "faces" / "night_window"
OUT_PREVIEW = HERE / "out"
W, H = 480, 320

FRAME = 8                      # rama okna (ciemna obwodka)
LAMP_X, LAMP_TOP = 330, 70     # slup latarni
BULB = (306, 77)
STREET_Y0, STREET_Y1 = 190, 236
LIT_WINDOW = (186, 146, 204, 162)

RED = (255, 40, 25)
RED_CORE = (255, 150, 130)
DIGIT_PX = 80
DIGIT_Y = 12
DATE_PX = 22
DATE_Y = 104

WEEKDAYS = ["NIEDZIELA", "PONIEDZIAŁEK", "WTOREK", "ŚRODA", "CZWARTEK", "PIĄTEK", "SOBOTA"]
DATE_CHARSET = sorted(set("".join(WEEKDAYS) + "0123456789. "))

# droga postaci: (t, x stop, y stop, skala); skala 1 = 62 px wzrostu
PATH = [(0.00, 306, 214, 0.78), (0.35, 262, 233, 0.98), (0.55, 232, 254, 1.30),
        (0.80, 238, 292, 2.15), (1.00, 242, 378, 4.30)]

BAYER4 = np.array([[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]])


def font(name, px):
    return ImageFont.truetype(str(FONTS / name), px)


def mask(fn, blur=0, size=(W, H)):
    m = Image.new("L", size, 0)
    fn(ImageDraw.Draw(m))
    if blur:
        m = m.filter(ImageFilter.GaussianBlur(blur))
    return np.asarray(m, np.float32) / 255


def over(a, m, rgb, k=1.0):
    a[:] = a * (1 - m[..., None] * k) + np.array(rgb, np.float32) * m[..., None] * k


def dither(a):
    t = np.tile(BAYER4, (H // 4 + 1, W // 4 + 1))[:H, :W][..., None] / 16.0
    steps = np.array([8, 4, 8])
    return np.clip(np.floor(np.clip(a + t * steps, 0, 255) / steps) * steps, 0, 255)


def to565(rgb):
    r, g, b = (rgb[..., i].astype(np.uint16) for i in range(3))
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


# ---------------------------------------------------------------- tlo
def background(lamp_on):
    rnd = random.Random(11)
    yy, xx = np.mgrid[0:H, 0:W].astype(np.float32)
    a = np.zeros((H, W, 3), np.float32)
    # niebo z luna miasta nad dachami
    t = np.clip(yy / 150, 0, 1)
    a[:] = (np.array([10, 12, 24]) * (1 - t)[..., None] + np.array([44, 38, 48]) * t[..., None])

    def houses(d):
        d.polygon([(0, 192), (0, 118), (62, 88), (126, 118), (126, 192)], fill=255)
        d.rectangle([90, 92, 102, 112], fill=255)                                   # komin
        d.polygon([(146, 192), (146, 128), (214, 96), (284, 128), (284, 192)], fill=255)
        d.polygon([(380, 192), (380, 110), (436, 84), (492, 110), (492, 192)], fill=255)
        d.rectangle([292, 150, 372, 192], fill=255)                                 # garaz
    hm = mask(houses, 0.6)
    over(a, hm, (22, 22, 30))
    # krawedzie dachow lekko odbijaja lune
    edge = np.clip(hm - mask(houses, 2.5), 0, 1)
    a += edge[..., None] * np.array([30, 26, 30])
    for (x0, y0, x1, y1) in [(20, 140, 34, 154), (84, 150, 98, 164), (410, 136, 424, 150), (452, 150, 466, 164)]:
        over(a, mask(lambda d: d.rectangle([x0, y0, x1, y1], fill=255)), (30, 30, 40))
    over(a, mask(lambda d: d.rectangle(LIT_WINDOW, fill=255)), (170, 128, 60))      # zapalone okno
    # nagie drzewo po lewej
    def branch(d, x, y, ang, ln, w, depth):
        if depth == 0:
            return
        nx, ny = x + math.cos(ang) * ln, y + math.sin(ang) * ln
        d.line([(x, y), (nx, ny)], fill=255, width=max(1, int(w)))
        for _ in range(2):
            branch(d, nx, ny, ang + rnd.uniform(-0.6, 0.6), ln * 0.72, w * 0.65, depth - 1)
    over(a, mask(lambda d: branch(d, 136, 236, -math.pi / 2 + 0.05, 52, 6, 7)), (10, 10, 14))

    # ulica, chodnik, plot, ogrodek
    a[STREET_Y0:STREET_Y1] = np.array([20, 20, 25])
    a[STREET_Y1:246] = np.array([36, 35, 38])
    yard = np.clip((yy - 246) / 74, 0, 1)
    spill = np.exp(-((xx - 240) / 260) ** 2) * yard ** 1.4          # swiatlo z pokoju
    a[246:] = (np.array([16, 20, 14]) + spill[246:, :, None] * np.array([86, 70, 44]))

    def fence(d):
        for x in range(0, W, 13):
            if 196 <= x <= 270:   # furtka otwarta
                continue
            d.polygon([(x, 262), (x, 240), (x + 4, 236), (x + 8, 240), (x + 8, 262)], fill=255)
        d.rectangle([0, 244, 196, 247], fill=255)
        d.rectangle([272, 244, W, 247], fill=255)
    fm = mask(fence)
    over(a, fm, (14, 13, 14))
    a += fm[..., None] * spill[..., None] * np.array([40, 32, 20])  # podswietlone od dolu

    # latarnia
    over(a, mask(lambda d: (d.line([(LAMP_X, 236), (LAMP_X, LAMP_TOP)], fill=255, width=3),
                            d.line([(LAMP_X, LAMP_TOP + 2), (BULB[0] - 2, LAMP_TOP + 2)], fill=255, width=3),
                            d.polygon([(BULB[0] - 8, BULB[1] - 4), (BULB[0] + 8, BULB[1] - 4), (BULB[0] + 5, BULB[1] + 1),
                                       (BULB[0] - 5, BULB[1] + 1)], fill=255))), (18, 18, 20))
    if lamp_on:
        cone = np.clip(1 - np.abs(xx - BULB[0]) / (16 + (yy - BULB[1]).clip(0) * 0.55), 0, 1) \
            * (yy > BULB[1]) * (yy < STREET_Y1 + 6)
        a += (cone * 0.28)[..., None] * np.array([255, 150, 60])
        pool = np.exp(-(((xx - BULB[0]) / 74) ** 2 + ((yy - 222) / 15) ** 2))
        a += (pool * 0.75)[..., None] * np.array([255, 140, 50])
        # odbicie na mokrym asfalcie
        refl = np.exp(-(((xx - BULB[0]) / 9) ** 2)) * ((yy > 214) & (yy < STREET_Y1)) * 0.5
        a += refl[..., None] * np.array([255, 150, 70])
        glow = np.exp(-(((xx - BULB[0]) / 13) ** 2 + ((yy - BULB[1]) / 8) ** 2))
        a += glow[..., None] * np.array([255, 195, 120]) * 1.5
        # lekkie oswietlenie garazu i slupa
        a += (np.exp(-(((xx - 330) / 60) ** 2 + ((yy - 170) / 40) ** 2)) * 0.25)[..., None] * np.array([255, 150, 60])

    # zaparowana szyba: delikatne rozjasnienie + winieta + ziarno
    a = a * 0.92 + 8
    a *= (1 - 0.42 * (np.hypot((xx - W / 2) / (W / 1.5), (yy - H / 2) / (H / 1.3))) ** 2)[..., None]
    a += np.random.default_rng(5).normal(0, 4, (H, W, 1))
    # rama okna
    over(a, mask(lambda d: (d.rectangle([0, 0, W, FRAME - 2], fill=255), d.rectangle([0, H - FRAME, W, H], fill=255),
                            d.rectangle([0, 0, FRAME, H], fill=255), d.rectangle([W - FRAME, 0, W, H], fill=255))), (6, 6, 7))
    return a


# ---------------------------------------------------------------- postac
def path_at(t):
    for (t0, x0, y0, s0), (t1, x1, y1, s1) in zip(PATH, PATH[1:]):
        if t <= t1:
            u = (t - t0) / (t1 - t0)
            u = u * u * (3 - 2 * u)
            return x0 + (x1 - x0) * u, y0 + (y1 - y0) * u, s0 + (s1 - s0) * u
    return PATH[-1][1:]


def figure_masks(minute):
    """Sylwetka w plaszczu z kapturem (wzrost 62*s px) + blysk oczu z bliska."""
    t = minute / 59
    fx, fy, s = path_at(t)
    k = 62 * s / 100.0   # jednostki rysunku: 100 = wzrost

    def P(x, y):
        return (fx + x * k, fy - y * k)

    def body(d):
        d.ellipse([*P(-6.5, 100), *P(6.5, 84)], fill=255)                    # glowa / kaptur
        d.polygon([P(-7, 90), P(-12, 82), P(-13, 70), P(-15, 42), P(-13, 30), P(-10, 30), P(-9, 50),
                   P(-8, 32), P(-6, 0), P(-1, 0), P(0, 30), P(1, 0), P(6, 0), P(8, 32), P(9, 50), P(10, 30),
                   P(13, 30), P(15, 42), P(13, 70), P(12, 82), P(7, 90)], fill=255)
    sil = mask(body, max(0.5, 0.5 * s))
    eyes = np.zeros_like(sil)
    if t > 0.7:
        e = mask(lambda d: (d.ellipse([*P(-3.4, 93), *P(-1.6, 92)], fill=255),
                            d.ellipse([*P(1.4, 93), *P(3.2, 92)], fill=255)), max(0.6, 0.35 * s))
        eyes = np.clip(e * 1.6 * (t - 0.7) / 0.3, 0, 1)
    # tylko wewnatrz ramy okna
    clip = np.zeros_like(sil)
    clip[FRAME - 2:H - FRAME, FRAME:W - FRAME] = 1
    return sil * clip, eyes * clip


FIG_DARK = (5, 5, 7)
FIG_EYE = (255, 225, 180)


def fig_frame(minute):
    sil, eyes = figure_masks(minute)
    a = np.maximum(sil, eyes)
    ys, xs = np.nonzero(a > 0.03)
    x0, x1, y0, y1 = xs.min(), xs.max() + 1, ys.min(), ys.max() + 1
    q = (np.round(eyes[y0:y1, x0:x1] * 15).astype(np.uint8) << 4) | np.round(sil[y0:y1, x0:x1] * 15).astype(np.uint8)
    return x0, y0, x1 - x0, y1 - y0, q


def fig_lut():
    c, al = [], []
    for idx in range(256):
        e, s_ = (idx >> 4) / 15, (idx & 15) / 15
        rgb = [FIG_DARK[i] * (1 - e) + FIG_EYE[i] * e for i in range(3)]
        c.append(((int(rgb[0]) & 0xF8) << 8) | ((int(rgb[1]) & 0xFC) << 3) | (int(rgb[2]) >> 3))
        al.append(int(round(max(e, s_) * 255)))
    return c, al


# ---------------------------------------------------------------- odbicie budzika (swiatlo dodawane)
GLOW_RADII = ((6, 0.5), (2, 0.8))
DATE_RADII = ((2, 0.4),)


def q4(x):
    return np.clip(np.round(x * 15), 0, 15).astype(np.uint8)


def glow_glyph(fnt, ch, cw, chh, xo, yo, radii):
    m = Image.new("L", (cw, chh), 0)
    ImageDraw.Draw(m).text((xo, yo), ch, font=fnt, fill=255)
    core = np.asarray(m, np.float32) / 255
    imax = sum(k for _, k in radii) * 1.6
    g = np.zeros_like(core)
    for r, k in radii:
        g += np.asarray(m.filter(ImageFilter.GaussianBlur(r)), np.float32) / 255 * k * 1.6
    return (q4(core) << 4) | q4(np.sqrt(np.clip(g / imax, 0, 1)))


def add_lut(tube, core_rgb, radii, core_k=0.8):
    imax = sum(k for _, k in radii) * 1.6
    out = []
    for idx in range(256):
        c = (idx >> 4) / 15 * core_k
        i = ((idx & 15) / 15) ** 2 * imax * 0.55
        out.append(tuple(int(min(255, core_rgb[k] * c + tube[k] * i)) for k in range(3)))
    return out


# ---------------------------------------------------------------- eksport
def c_array(name, data, ctype="uint8_t", per_line=32, dims=None, static=False):
    vals = [str(int(v)) for v in np.asarray(data).flatten()]
    lines = [", ".join(vals[i:i + per_line]) for i in range(0, len(vals), per_line)]
    pre = "static const" if static else "const"
    return f"{pre} {ctype} {name}{dims or f'[{len(vals)}]'} = {{\n  " + ",\n  ".join(lines) + "\n};\n"


def main():
    OUT_SRC.mkdir(parents=True, exist_ok=True)
    OUT_PREVIEW.mkdir(exist_ok=True)

    bg_on, bg_off = dither(background(True)), dither(background(False))
    diff = np.abs(bg_on - bg_off).max(axis=2) > 8
    ys, xs = np.nonzero(diff)
    lx0, lx1, ly0, ly1 = xs.min(), xs.max() + 1, ys.min(), ys.max() + 1

    # okno naprzeciwko: kolor po zgaszeniu = kolor domu
    dark_win = to565(bg_on[LIT_WINDOW[1] - 3:LIT_WINDOW[1] - 2, LIT_WINDOW[0]:LIT_WINDOW[0] + 1])[0, 0]

    # cyfry DSEG7 - jednakowa komorka
    f = font("DSEG7Classic-Bold.ttf", DIGIT_PX)
    pad = 12
    b8 = f.getbbox("8")
    cw, chh = (b8[2] - b8[0]) + 2 * pad, (b8[3] - b8[1]) + 2 * pad
    digits = [glow_glyph(f, c, cw, chh, pad - b8[0], pad - b8[1], GLOW_RADII) for c in "0123456789"]
    adv = f.getlength("8")
    colon_adv = f.getlength(":")
    total = 4 * adv + colon_adv
    tx = (W - total) / 2
    xs_slot = [round(tx + k * adv) - pad for k in (0, 1)] + [round(tx + 2 * adv + colon_adv + k * adv) - pad for k in (0, 1)]
    cb = f.getbbox(":")
    colon_w = (cb[2] - cb[0]) + 2 * pad
    colon = glow_glyph(f, ":", colon_w, chh, pad - cb[0], pad - b8[1], GLOW_RADII)
    colon_x = round(tx + 2 * adv + cb[0]) - pad
    lut_red = add_lut(RED, RED_CORE, GLOW_RADII)

    # data VT323
    fd = font("VT323.ttf", DATE_PX)
    asc, _ = fd.getmetrics()
    dglyphs = []
    for ch in DATE_CHARSET:
        a_ = int(round(fd.getlength(ch)))
        bb = fd.getbbox(ch)
        if ch == " " or bb[2] <= bb[0]:
            dglyphs.append(dict(cp=32, w=0, h=0, xoff=0, yoff=0, adv=a_, data=np.zeros(0, np.uint8)))
            continue
        p = 3
        w_, h_ = bb[2] - bb[0] + 2 * p, bb[3] - bb[1] + 2 * p
        dglyphs.append(dict(cp=ord(ch), w=w_, h=h_, xoff=bb[0] - p, yoff=bb[1] - asc - p, adv=a_,
                            data=glow_glyph(fd, ch, w_, h_, p - bb[0], p - bb[1], DATE_RADII)))
    lut_date = add_lut((200, 40, 30), (230, 90, 70), DATE_RADII, core_k=0.55)

    figs = [fig_frame(m) for m in range(60)]
    flut_c, flut_a = fig_lut()

    # krople na szybie (stale): x, y, r, jasnosc
    rnd = random.Random(21)
    drops = [(rnd.randint(FRAME + 2, W - FRAME - 3), rnd.randint(FRAME, H - FRAME - 3), rnd.choice((1, 1, 2, 2, 3)),
              rnd.randint(18, 46)) for _ in range(130)]

    # ---- C++
    hdr = f"""#pragma once
// WYGENEROWANE przez tools/prerender/night_window/gen_assets.py - nie edytowac recznie.
#include <stdint.h>

namespace assets {{

constexpr int FRAME = {FRAME};
constexpr int STREET_Y0 = {STREET_Y0}, STREET_Y1 = {STREET_Y1};

extern const uint16_t BG[{H} * {W}];
constexpr int LAMP_X = {lx0}, LAMP_Y = {ly0}, LAMP_W = {lx1 - lx0}, LAMP_H = {ly1 - ly0};
extern const uint16_t LAMP_OFF[LAMP_W * LAMP_H];

// zapalone okno naprzeciwko (gasnie/zapala sie)
constexpr int LIT_X0 = {LIT_WINDOW[0]}, LIT_Y0 = {LIT_WINDOW[1]}, LIT_X1 = {LIT_WINDOW[2] + 1}, LIT_Y1 = {LIT_WINDOW[3] + 1};
constexpr uint16_t LIT_DARK = {dark_win};

// postac: klatka = minuta, piksel = (oczy4 << 4) | krycie4
struct FigFrame {{ int16_t x, y, w, h; const uint8_t* data; }};
extern const FigFrame FIGURE[60];
extern const uint16_t FIG_LUT_COLOR[256];
extern const uint8_t FIG_LUT_ALPHA[256];

// odbicie budzika: index = (core4 << 4) | glow4 -> RGB888 dodawane do tla
constexpr int DIGIT_CELL_W = {cw}, DIGIT_CELL_H = {chh}, DIGIT_Y = {DIGIT_Y - pad + 2};
constexpr int DIGIT_X[4] = {{{', '.join(map(str, xs_slot))}}};
constexpr int COLON_X = {colon_x}, COLON_W = {colon_w};
extern const uint8_t DIGIT_DATA[10][DIGIT_CELL_W * DIGIT_CELL_H];
extern const uint8_t COLON_DATA[COLON_W * DIGIT_CELL_H];
extern const uint8_t LUT_RED[256][3];

struct DateGlyph {{ uint16_t cp; uint8_t w, h; int8_t xoff, yoff; uint8_t adv; uint32_t offset; }};
constexpr int DATE_BASELINE = {DATE_Y + asc}, DATE_CENTER_X = {W // 2};
constexpr int DATE_GLYPH_COUNT = {len(dglyphs)};
extern const DateGlyph DATE_GLYPHS[DATE_GLYPH_COUNT];
extern const uint8_t DATE_DATA[];
extern const uint8_t LUT_DATE[256][3];
extern const char* const WEEKDAYS[7];

// krople na szybie
struct Drop {{ int16_t x, y; uint8_t r, k; }};
constexpr int DROP_COUNT = {len(drops)};
extern const Drop DROPS[DROP_COUNT];

}}  // namespace assets
"""
    cpp = ["// WYGENEROWANE przez tools/prerender/night_window/gen_assets.py - nie edytowac recznie.",
           '#include "assets_gen.h"', "", "namespace assets {", ""]
    cpp.append(c_array("BG", to565(bg_on), "uint16_t", 24, f"[{H} * {W}]"))
    cpp.append(c_array("LAMP_OFF", to565(bg_off[ly0:ly1, lx0:lx1]), "uint16_t", 24, "[LAMP_W * LAMP_H]"))
    for i, (x, y, w_, h_, q) in enumerate(figs):
        cpp.append(c_array(f"F{i}", q, static=True))
    cpp.append("const FigFrame FIGURE[60] = {\n" + "\n".join(
        f"  {{{x}, {y}, {w_}, {h_}, F{i}}}," for i, (x, y, w_, h_, _) in enumerate(figs)) + "\n};\n")
    cpp.append(c_array("FIG_LUT_COLOR", flut_c, "uint16_t", 16))
    cpp.append(c_array("FIG_LUT_ALPHA", flut_a))
    cpp.append("const uint8_t DIGIT_DATA[10][DIGIT_CELL_W * DIGIT_CELL_H] = {")
    cpp += ["  {" + ",".join(map(str, d.flatten())) + "}," for d in digits] + ["};\n"]
    cpp.append(c_array("COLON_DATA", colon))
    cpp.append(c_array("LUT_RED", [v for e in lut_red for v in e], dims="[256][3]"))
    off, data, ent = 0, [], []
    for g in dglyphs:
        ent.append(f"  {{{g['cp']}, {g['w']}, {g['h']}, {g['xoff']}, {g['yoff']}, {g['adv']}, {off}}},")
        fl = g["data"].flatten()
        data.extend(fl)
        off += len(fl)
    cpp.append("const DateGlyph DATE_GLYPHS[DATE_GLYPH_COUNT] = {\n" + "\n".join(ent) + "\n};\n")
    cpp.append(c_array("DATE_DATA", data, dims="[]"))
    cpp.append(c_array("LUT_DATE", [v for e in lut_date for v in e], dims="[256][3]"))
    cpp.append("const char* const WEEKDAYS[7] = {" + ", ".join(f'"{d}"' for d in WEEKDAYS) + "};\n")
    cpp.append("const Drop DROPS[DROP_COUNT] = {\n" + "\n".join(f"  {{{x}, {y}, {r}, {k}}}," for x, y, r, k in drops) + "\n};\n")
    cpp.append("}  // namespace assets")
    (OUT_SRC / "assets_gen.h").write_text(hdr, encoding="utf-8")
    (OUT_SRC / "assets_gen.cpp").write_text("\n".join(cpp) + "\n", encoding="utf-8")

    fig_px = sum(w_ * h_ for _, _, w_, h_, _ in figs)
    print(f"lampa {lx1 - lx0}x{ly1 - ly0} @ {lx0},{ly0}; cyfry {cw}x{chh} x={xs_slot}; postac {fig_px} px")

    # ---- podglad (jak zlozy ESP32)
    for minute, lamp, name in ((12, True, "preview_m12"), (50, True, "preview_m50"), (50, False, "preview_m50_off")):
        img = (bg_on if lamp else bg_off).copy()
        x, y, w_, h_, q = figs[minute]
        c = np.array(flut_c)[q]
        al = np.array(flut_a)[q][..., None] / 255
        rgb = np.stack([(c >> 11) << 3, ((c >> 5) & 63) << 2, (c & 31) << 3], -1)
        img[y:y + h_, x:x + w_] = img[y:y + h_, x:x + w_] * (1 - al) + rgb * al
        for dx, dy, r, k in drops:
            img[dy - r:dy + r + 1, dx - r:dx + r + 1] += k * 0.6
        for i, dg in enumerate((2, 3, minute // 10, minute % 10)):
            d = digits[dg]
            img[DIGIT_Y - pad + 2:DIGIT_Y - pad + 2 + chh, xs_slot[i]:xs_slot[i] + cw] += np.array(lut_red)[d]
        img[DIGIT_Y - pad + 2:DIGIT_Y - pad + 2 + chh, colon_x:colon_x + colon_w] += np.array(lut_red)[colon]
        x = W // 2 - sum(next(g for g in dglyphs if g["cp"] == ord(ch))["adv"] for ch in "PIĄTEK 02.10.2026") // 2
        for ch in "PIĄTEK 02.10.2026":
            g = next(g for g in dglyphs if g["cp"] == ord(ch))
            if g["w"]:
                gy = DATE_Y + asc + g["yoff"]
                img[gy:gy + g["h"], x + g["xoff"]:x + g["xoff"] + g["w"]] += np.array(lut_date)[g["data"]]
            x += g["adv"]
        Image.fromarray(np.clip(img, 0, 255).astype(np.uint8)).resize((W * 2, H * 2), Image.NEAREST).save(
            OUT_PREVIEW / f"{name}.png")


if __name__ == "__main__":
    main()
