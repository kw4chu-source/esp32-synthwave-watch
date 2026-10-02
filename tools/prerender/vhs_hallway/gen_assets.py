"""Zasoby tarczy "vhs_hallway" - nagranie VHS z korytarza, postac zbliza sie
z kazda minuta.

Geometria i rysunek z mockup.py (jeden wyglad dla makiety i firmware).
Na laptopie liczymy: tlo z efektami VHS, wariant tla przy zgaszonej
swietlowce (tylko zmieniony fragment), 60 klatek postaci (po jednej na
minute), cyfry i znaki daty z cieniem i rozjechanymi kanalami.

  python tools/prerender/vhs_hallway/gen_assets.py
"""

from __future__ import annotations

import random
import sys
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFilter

sys.path.insert(0, str(Path(__file__).parent))
import mockup as mk  # noqa: E402

HERE = Path(__file__).parent
ROOT = HERE.parents[2]
OUT_SRC = ROOT / "src" / "faces" / "vhs_hallway"
OUT_PREVIEW = HERE / "out"
W, H = mk.W, mk.H

GRAIN_SEED = 1984
TEXT_RGB = (240, 240, 235)
FLICKER_LAMP = 1              # ktora swietlowka migocze (indeks w mk.corridor)

DIGIT_PX = 118
DIGIT_Y = 12                  # gorna krawedz cyfr
DATE_PX = 26
DATE_X, DATE_Y = 16, H - 34
REC_DOT = (16, 14, 30, 28)

WEEKDAYS = ["NIEDZIELA", "PONIEDZIAŁEK", "WTOREK", "ŚRODA", "CZWARTEK", "PIĄTEK", "SOBOTA"]
DATE_CHARSET = sorted(set("".join(WEEKDAYS) + "0123456789. "))


# ---------------------------------------------------------------- VHS
def scan_factor(y):
    return 0.86 if y % 2 == 0 else 1.0


def vhs_static(arr):
    """Efekty "zapieczone" w tle: kanaly, ziarno, linie, winieta, kolor."""
    out = arr.copy()
    out[:, 2:, 0] = arr[:, :-2, 0]
    out[:, :-1, 2] = arr[:, 1:, 2]
    out += np.random.default_rng(GRAIN_SEED).normal(0, 9, out.shape)
    out[::2] *= 0.86
    yy, xx = np.mgrid[0:H, 0:W]
    out *= (1 - 0.45 * (np.hypot((xx - W / 2) / (W / 1.6), (yy - H / 2) / (H / 1.4))) ** 2)[..., None]
    out *= np.array([0.92, 1.0, 0.95])
    return out


def static_osd(arr):
    """Napisy, ktore sie nie zmieniaja: REC, SP >, CH 03 (z cieniem)."""
    o = Image.new("L", (W, H), 0)
    d = ImageDraw.Draw(o)
    small = mk.font(DATE_PX)
    d.text((38, 8), "REC", font=small, fill=255)
    d.text((W - 72, 8), "SP", font=small, fill=255)
    d.polygon([(W - 34, 14), (W - 34, 30), (W - 20, 22)], fill=255)
    d.text((W - 118, H - 34), "CH 03", font=small, fill=255)
    put_text(arr, o)


def put_text(arr, o):
    om = np.asarray(o, np.float32)[..., None] / 255
    sh = np.asarray(o.filter(ImageFilter.MaxFilter(5)).filter(ImageFilter.GaussianBlur(2)), np.float32)[..., None] / 255
    arr[:] = arr * (1 - sh * 0.75)
    arr[:] = arr * (1 - om) + np.array(TEXT_RGB, np.float32) * om


BAYER4 = np.array([[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]])


def dither(arr, y0=0, x0=0):
    h, w = arr.shape[:2]
    t = np.tile(BAYER4, (h // 4 + 2, w // 4 + 2))[y0 % 4:y0 % 4 + h, x0 % 4:x0 % 4 + w][..., None] / 16.0
    steps = np.array([8, 4, 8])
    return np.clip(np.floor(np.clip(arr + t * steps, 0, 255) / steps) * steps, 0, 255)


def to565(rgb):
    r, g, b = rgb[..., 0].astype(np.uint16), rgb[..., 1].astype(np.uint16), rgb[..., 2].astype(np.uint16)
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


# ---------------------------------------------------------------- warstwy RGBA
def rgba_layer(draw_mask_fn, color, shadow=True, chroma=True):
    """Bialy tekst/ksztalt z cieniem i rozjechanymi kanalami jak na tasmie.
    Zwraca (rgb, alpha) w pelnym kadrze."""
    o = Image.new("L", (W, H), 0)
    draw_mask_fn(ImageDraw.Draw(o))
    m = np.asarray(o, np.float32) / 255
    sh = np.asarray(o.filter(ImageFilter.MaxFilter(5)).filter(ImageFilter.GaussianBlur(2)),
                    np.float32) / 255 * 0.75 if shadow else np.zeros_like(m)
    rgb = np.zeros((H, W, 3), np.float32)
    alpha = np.maximum(sh, m)
    rgb[:] = np.array(color, np.float32) * m[..., None]   # cien = czern
    if chroma:  # czerwony 2 px w prawo, niebieski 1 px w lewo
        r = np.zeros_like(m)
        r[:, 2:] = m[:, :-2]
        b = np.zeros_like(m)
        b[:, :-1] = m[:, 1:]
        rgb[..., 0] = np.maximum(rgb[..., 0], r * color[0] * 0.8)
        rgb[..., 2] = np.maximum(rgb[..., 2], b * color[2] * 0.8)
        alpha = np.maximum(alpha, np.maximum(r, b) * 0.8)
    with np.errstate(invalid="ignore", divide="ignore"):
        rgb = np.where(alpha[..., None] > 0, rgb / np.maximum(alpha[..., None], 1e-6), 0)
    rgb *= np.array([scan_factor(y) for y in range(H)])[:, None, None]
    return rgb, alpha


def crop(rgb, alpha, thr=0.02):
    ys, xs = np.nonzero(alpha > thr)
    if len(xs) == 0:
        return None
    x0, x1, y0, y1 = xs.min(), xs.max() + 1, ys.min(), ys.max() + 1
    return x0, y0, x1 - x0, y1 - y0, rgb[y0:y1, x0:x1], alpha[y0:y1, x0:x1]


# ---------------------------------------------------------------- postac
def figure_layer(minute):
    """Sylwetka (w pelni kryjaca, miekkie krawedzie) + swiecace oczy jako
    jeden bajt na piksel: (oczy4 << 4) | krycie4 -> FIG_LUT."""
    z = 1.15 + (mk.Z_END * 0.97 - 1.15) * (1 - minute / 59) ** 1.6
    base = np.zeros((H, W, 3), np.float32)
    mk.figure(base, z, random.Random(0))       # na czerni: widac tylko oczy
    white = np.full((H, W, 3), 255, np.float32)
    mk.figure(white, z, random.Random(0))       # na bieli: widac sylwetke
    sil = np.clip((1 - white[..., 1] / 255) / 0.9, 0, 1)
    eyes = np.clip(base[..., 0] / 255, 0, 1)
    a = np.maximum(sil, eyes)
    ys, xs = np.nonzero(a > 0.03)
    x0, x1, y0, y1 = xs.min(), xs.max() + 1, ys.min(), ys.max() + 1
    q = (np.round(eyes[y0:y1, x0:x1] * 15).astype(np.uint8) << 4) | np.round(sil[y0:y1, x0:x1] * 15).astype(np.uint8)
    return x0, y0, x1 - x0, y1 - y0, q


FIG_DARK = (4, 5, 5)
FIG_EYE = (255, 245, 220)


def fig_lut():
    c, a = [], []
    for idx in range(256):
        e, s_ = (idx >> 4) / 15, (idx & 15) / 15
        rgb = [FIG_DARK[k] * (1 - e) + FIG_EYE[k] * e for k in range(3)]
        c.append(((int(rgb[0]) & 0xF8) << 8) | ((int(rgb[1]) & 0xFC) << 3) | (int(rgb[2]) >> 3))
        a.append(int(round(max(e, s_) * 255)))
    return c, a


# ---------------------------------------------------------------- eksport
def c_array(name, data, ctype="uint8_t", per_line=32, dims=None):
    vals = [str(int(v)) for v in np.asarray(data).flatten()]
    lines = [", ".join(vals[i:i + per_line]) for i in range(0, len(vals), per_line)]
    return f"const {ctype} {name}{dims or f'[{len(vals)}]'} = {{\n  " + ",\n  ".join(lines) + "\n};\n"


def sprite_arrays(rgb, alpha, y0, x0):
    q = dither(rgb, y0, x0)
    return to565(q).flatten(), (np.clip(alpha, 0, 1) * 255).astype(np.uint8).flatten()


def main():
    OUT_SRC.mkdir(parents=True, exist_ok=True)
    OUT_PREVIEW.mkdir(exist_ok=True)

    # ---- tlo + wariant ze zgaszona swietlowka
    bg = mk.corridor(None)
    bg_off = mk.corridor(FLICKER_LAMP)
    bg, bg_off = vhs_static(bg), vhs_static(bg_off)
    static_osd(bg)
    static_osd(bg_off)
    bg_q, off_q = dither(bg), dither(bg_off)
    diff = np.abs(bg_q - off_q).max(axis=2) > 8
    ys, xs = np.nonzero(diff)
    lx0, lx1, ly0, ly1 = xs.min(), xs.max() + 1, ys.min(), ys.max() + 1
    lamp_off = to565(off_q[ly0:ly1, lx0:lx1])

    # ---- cyfry: jednakowa szerokosc (VT323 jest monospace)
    f = mk.font(DIGIT_PX)
    b0 = f.getbbox("0")
    adv = f.getlength("0")
    total = f.getlength("00:00")
    tx = (W - total) / 2
    ty = DIGIT_Y - b0[1] + 8
    digit_sprites = []
    for i, ch in enumerate("0123456789:"):
        x = tx if ch != ":" else tx + 2 * adv
        rgb, a = rgba_layer(lambda d: d.text((x, ty), ch, font=f, fill=255), TEXT_RGB)
        digit_sprites.append(crop(rgb, a))
    # wspolna komorka dla cyfr (pozycja x zalezy od slotu)
    dx0 = min(s[0] for s in digit_sprites[:10])
    dy0 = min(s[1] for s in digit_sprites[:10])
    dx1 = max(s[0] + s[2] for s in digit_sprites[:10])
    dy1 = max(s[1] + s[3] for s in digit_sprites[:10])
    cell_w, cell_h = dx1 - dx0, dy1 - dy0
    cells = []
    for ch in "0123456789":
        rgb, a = rgba_layer(lambda d: d.text((tx, ty), ch, font=f, fill=255), TEXT_RGB)
        cells.append(sprite_arrays(rgb[dy0:dy1, dx0:dx1], a[dy0:dy1, dx0:dx1], dy0, dx0))
    slot_x = [round(dx0 + k * adv) for k in (0, 1, 3, 4)]
    cx0, cy0, cw, chh, crgb, ca = digit_sprites[10]
    colon = sprite_arrays(crgb, ca, cy0, cx0)

    # ---- znaki daty
    fs = mk.font(DATE_PX)
    ascent, _ = fs.getmetrics()
    date_glyphs = []
    for ch in DATE_CHARSET:
        adv_c = int(round(fs.getlength(ch)))
        if ch == " ":
            date_glyphs.append(dict(cp=32, w=0, h=0, xoff=0, yoff=0, adv=adv_c, c=[], a=[]))
            continue
        rgb, a = rgba_layer(lambda d: d.text((100, DATE_Y), ch, font=fs, fill=255), TEXT_RGB)
        x0, y0, w, h, r2, a2 = crop(rgb, a)
        c, al = sprite_arrays(r2, a2, y0, x0)
        date_glyphs.append(dict(cp=ord(ch), w=w, h=h, xoff=x0 - 100, yoff=y0 - DATE_Y, adv=adv_c, c=c, a=al))

    # ---- kropka REC
    rgb, a = rgba_layer(lambda d: d.ellipse(REC_DOT, fill=255), (235, 30, 30), chroma=False)
    rx, ry, rw, rh, rr, ra = crop(rgb, a)
    rec = sprite_arrays(rr, ra, ry, rx)

    # ---- 60 klatek postaci
    figs = [figure_layer(m) for m in range(60)]
    flut_c, flut_a = fig_lut()

    # ---------------------------------------------------------------- C++
    hdr = [f"""#pragma once
// WYGENEROWANE przez tools/prerender/vhs_hallway/gen_assets.py - nie edytowac recznie.
#include <stdint.h>

namespace assets {{

// Sprite RGBA: kolor RGB565 + alfa 0-255, pozycja na ekranie
struct Sprite {{ int16_t x, y, w, h; const uint16_t* color; const uint8_t* alpha; }};

extern const uint16_t BG[{H} * {W}];

// fragment tla przy zgaszonej swietlowce
constexpr int LAMP_X = {lx0}, LAMP_Y = {ly0}, LAMP_W = {lx1 - lx0}, LAMP_H = {ly1 - ly0};
extern const uint16_t LAMP_OFF[LAMP_W * LAMP_H];

// cyfry: wspolna komorka, x zalezy od slotu
constexpr int DIGIT_CELL_W = {cell_w}, DIGIT_CELL_H = {cell_h}, DIGIT_Y = {dy0};
constexpr int DIGIT_X[4] = {{{', '.join(map(str, slot_x))}}};
extern const uint16_t DIGIT_COLOR[10][DIGIT_CELL_W * DIGIT_CELL_H];
extern const uint8_t DIGIT_ALPHA[10][DIGIT_CELL_W * DIGIT_CELL_H];
extern const Sprite COLON;

// data (VT323 {DATE_PX}px)
struct DateGlyph {{ uint16_t cp; uint8_t w, h; int8_t xoff, yoff; uint8_t adv; const uint16_t* color; const uint8_t* alpha; }};
constexpr int DATE_X = {DATE_X}, DATE_Y = {DATE_Y};
constexpr int DATE_GLYPH_COUNT = {len(date_glyphs)};
extern const DateGlyph DATE_GLYPHS[DATE_GLYPH_COUNT];
extern const char* const WEEKDAYS[7];

extern const Sprite REC_DOT;

// postac: klatka na kazda minute (0 = koniec korytarza, 59 = przed kamera)
// piksel = (oczy4 << 4) | krycie4 -> FIG_LUT_COLOR / FIG_LUT_ALPHA
struct FigFrame {{ int16_t x, y, w, h; const uint8_t* data; }};
extern const FigFrame FIGURE[60];
extern const uint16_t FIG_LUT_COLOR[256];
extern const uint8_t FIG_LUT_ALPHA[256];

}}  // namespace assets
"""]
    cpp = ["// WYGENEROWANE przez tools/prerender/vhs_hallway/gen_assets.py - nie edytowac recznie.",
           '#include "assets_gen.h"', "", "namespace assets {", ""]
    cpp.append(c_array("BG", to565(bg_q), "uint16_t", 24, f"[{H} * {W}]"))
    cpp.append(c_array("LAMP_OFF", lamp_off, "uint16_t", 24, "[LAMP_W * LAMP_H]"))
    cpp.append("const uint16_t DIGIT_COLOR[10][DIGIT_CELL_W * DIGIT_CELL_H] = {")
    cpp += ["  {" + ",".join(map(str, c)) + "}," for c, _ in cells] + ["};\n"]
    cpp.append("const uint8_t DIGIT_ALPHA[10][DIGIT_CELL_W * DIGIT_CELL_H] = {")
    cpp += ["  {" + ",".join(map(str, a)) + "}," for _, a in cells] + ["};\n"]

    def sprite(name, x, y, w, h, c, a):
        return (c_array(f"{name}_C", c, "static uint16_t") .replace("const static", "static const")
                + c_array(f"{name}_A", a, "static uint8_t").replace("const static", "static const")
                + f"const Sprite {name} = {{{x}, {y}, {w}, {h}, {name}_C, {name}_A}};\n")
    cpp.append(sprite("COLON", cx0, cy0, cw, chh, *colon))
    cpp.append(sprite("REC_DOT", rx, ry, rw, rh, *rec))
    for i, g in enumerate(date_glyphs):
        if g["w"]:
            cpp.append(c_array(f"DG{i}_C", g["c"], "static uint16_t").replace("const static", "static const"))
            cpp.append(c_array(f"DG{i}_A", g["a"], "static uint8_t").replace("const static", "static const"))
    cpp.append("const DateGlyph DATE_GLYPHS[DATE_GLYPH_COUNT] = {\n" + "\n".join(
        f"  {{{g['cp']}, {g['w']}, {g['h']}, {g['xoff']}, {g['yoff']}, {g['adv']}, "
        + (f"DG{i}_C, DG{i}_A" if g["w"] else "nullptr, nullptr") + "},"
        for i, g in enumerate(date_glyphs)) + "\n};\n")
    cpp.append("const char* const WEEKDAYS[7] = {" + ", ".join(f'"{d}"' for d in WEEKDAYS) + "};\n")
    for i, (x, y, w, h, q) in enumerate(figs):
        cpp.append(c_array(f"F{i}", q, "static uint8_t").replace("const static", "static const"))
    cpp.append("const FigFrame FIGURE[60] = {\n" + "\n".join(
        f"  {{{x}, {y}, {w}, {h}, F{i}}}," for i, (x, y, w, h, _) in enumerate(figs)) + "\n};\n")
    cpp.append(c_array("FIG_LUT_COLOR", flut_c, "uint16_t", 16))
    cpp.append(c_array("FIG_LUT_ALPHA", flut_a))
    cpp.append("}  // namespace assets")
    (OUT_SRC / "assets_gen.h").write_text("".join(hdr), encoding="utf-8")
    (OUT_SRC / "assets_gen.cpp").write_text("\n".join(cpp) + "\n", encoding="utf-8")

    fig_px = sum(w * h for _, _, w, h, _ in figs)
    total_b = W * H * 2 + lamp_off.size * 2 + 10 * cell_w * cell_h * 3 + fig_px
    print(f"lampa: {lx1 - lx0}x{ly1 - ly0} @ {lx0},{ly0}; cyfry {cell_w}x{cell_h} @ x={slot_x} y={dy0}")
    print(f"postac: 60 klatek, {fig_px} px; zasoby razem ~{total_b / 1024:.0f} KB")

    # ---- podglad z danych wyjsciowych (jak zlozy je ESP32)
    def over(img, x, y, w, h, c, a):
        c = np.asarray(c).reshape(h, w)
        a = np.asarray(a).reshape(h, w).astype(np.float32)[..., None] / 255
        rgb = np.stack([(c >> 11) << 3, ((c >> 5) & 63) << 2, (c & 31) << 3], -1).astype(np.float32)
        img[y:y + h, x:x + w] = img[y:y + h, x:x + w] * (1 - a) + rgb * a
    for m, name in ((5, "preview_m05"), (52, "preview_m52")):
        img = bg_q.copy()
        x, y, w, h, q = figs[m]
        over(img, x, y, w, h, np.array(flut_c)[q.flatten()], np.array(flut_a)[q.flatten()])
        for k, dgt in enumerate((2, 3, m // 10, m % 10)):
            over(img, slot_x[k], dy0, cell_w, cell_h, *cells[dgt])
        over(img, cx0, cy0, cw, chh, *colon)
        over(img, rx, ry, rw, rh, *rec)
        x = DATE_X
        by_cp = {g["cp"]: g for g in date_glyphs}
        for ch in "PIĄTEK 02.10.2026":
            g = by_cp[ord(ch)]
            if g["w"]:
                over(img, x + g["xoff"], DATE_Y + g["yoff"], g["w"], g["h"], g["c"], g["a"])
            x += g["adv"]
        Image.fromarray(img.astype(np.uint8)).resize((W * 2, H * 2), Image.NEAREST).save(OUT_PREVIEW / f"{name}.png")


if __name__ == "__main__":
    main()
