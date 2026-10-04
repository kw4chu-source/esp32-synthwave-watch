"""Makiety (animowane GIF) - druga seria, bardziej realistyczna:
  E. Czarna dziura - dysk akrecyjny z soczewkowaniem (luk nad cieniem), rotacja roznicowa, Doppler
  F. Zorza         - kurtyny zorzy polarnej nad gorami, odbicie w jeziorze, gwiazdy
  G. Ognisko       - realistyczny ogien (symulacja), iskry, Droga Mleczna, sylwetki swierkow

  python tools/prerender/fun_options/mockups2.py  -> out/E_*.gif .. G_*.gif + out/sheet2.png
"""

from __future__ import annotations

import math

import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont

from mockups import FONTS, H, N, OUT, TIME, W, F, big_text, finish, glow

YS, XS = np.mgrid[0:H, 0:W].astype(np.float32)
DATE = "SOBOTA  04.10.2026"


def value_noise(seed, gw, gh, w=W, h=H):
    rng = np.random.default_rng(seed)
    small = Image.fromarray((rng.random((gh, gw)) * 255).astype(np.uint8))
    return np.asarray(small.resize((w, h), Image.BICUBIC), np.float32) / 255


def star_layer(seed, n, mask=None):
    rng = np.random.default_rng(seed)
    out = np.zeros((H, W), np.float32)
    xs, ys = rng.integers(0, W, n), rng.integers(0, H, n)
    out[ys, xs] = rng.random(n) ** 2
    return out, xs, ys


# ---------------------------------------------------------------- E. Czarna dziura
def blackhole_frames():
    cx, cy, rs = 240, 118, 36.0
    rin, rout = 54.0, 225.0
    dx, dy = XS - cx, YS - cy
    rho = np.hypot(dx, dy)
    psi = np.arctan2(dy, dx)
    inc = 0.14
    R = np.hypot(dx, dy / inc)
    th = np.arctan2(dy / inc, dx)
    # obraz soczewkowany: pierscien wokol cienia odpowiada dalszej czesci dysku
    lens_band = (rho > rs * 1.06) & (rho < rs * 2.5)
    Rl = rin + (rho - rs * 1.06) / (rs * 1.44) * (rout * 0.5 - rin)
    lens_w = np.where(dy < 0, 1.0, 0.45) * np.clip((rs * 2.5 - rho) / (rs * 0.6), 0, 1)
    stars, _, _ = star_layer(5, 260)
    stars *= (rho > rs * 2.6)

    def disk(Rr, ang, t):
        omega = (rin / np.maximum(Rr, rin)) ** 1.5
        a = ang - omega * t * 2 * math.pi * 1.2
        tex = 0.55 + 0.25 * np.sin(7 * a + Rr * 0.09) + 0.15 * np.sin(19 * a - Rr * 0.21) + 0.1 * np.sin(3 * a + Rr * 0.04)
        prof = np.exp(-(Rr - rin) / 55) * np.clip((Rr - rin) / 6, 0, 1) * (Rr < rout)
        dop = 1 - 0.55 * np.cos(ang)
        return np.clip(tex, 0, 1.3) * prof * dop

    def temp_color(I, Rr):
        hot = np.clip(1 - (Rr - rin) / 120, 0, 1)[..., None]
        c_out, c_in = np.array([255, 90, 20], np.float32), np.array([255, 235, 190], np.float32)
        return (c_out * (1 - hot) + c_in * hot) * I[..., None]

    frames = []
    font, fdate = F("Rajdhani-Bold.ttf", 104), F("Rajdhani-Bold.ttf", 22)
    for i in range(N):
        t = i / N
        img = np.zeros((H, W, 3), np.float32) + stars[..., None] * 220
        Il = disk(Rl, psi, t) ** 0.6 * lens_w * lens_band * 0.8
        img += temp_color(Il, Rl) * 1.1
        Id = disk(R, th, t)
        back = dy < 0
        img += temp_color(Id * back, R)
        img *= (rho > rs)[..., None]  # cien
        img += temp_color(Id * ~back, R)
        ring = np.exp(-((rho - rs * 1.03) / 1.4) ** 2)
        img += ring[..., None] * np.array([255, 220, 170], np.float32) * 0.9
        light = finish(np.clip(img, 0, 255))
        base = glow(light, 6, 0.5)
        im = finish(base)
        tl = Image.new("RGB", (W, H), 0)
        td = ImageDraw.Draw(tl)
        big_text(td, (240, 248), TIME, font, (255, 244, 225), (0, 0, 0), 0)
        im = finish(np.asarray(im, np.float32) + glow(tl, 6, 0.6) * np.array([1, 0.8, 0.55]))
        big_text(ImageDraw.Draw(im), (240, 303), DATE, fdate, (255, 150, 60), (0, 0, 0), 0)
        frames.append(im)
    return frames


# ---------------------------------------------------------------- F. Zorza
AURORA_STYLES = {
    "mieta": ("Rajdhani-Bold.ttf", 100, (120, 205, 170), 0.35),
    "neon": ("TiltNeon.ttf", 108, (70, 200, 150), 1.1),
    "lawenda": ("Oxanium.ttf", 92, (165, 150, 225), 0.45),
}


def aurora_frames(style="mieta", n=N):
    fname, fsize, tcol, tglow = AURORA_STYLES[style]
    horizon = 222
    sky = np.zeros((H, W, 3), np.float32)
    f = (YS / horizon)[..., None]
    sky += np.array([1, 3, 10]) * (1 - f) + np.array([6, 16, 34]) * f
    stars, sx, sy = star_layer(7, 220)
    # gory: zlozenie sinusow + szum
    xs = np.arange(W)
    ridge = (horizon - 18 - 30 * np.abs(np.sin(xs / 70 + 0.5)) - 18 * np.abs(np.sin(xs / 23 + 2))
             - 6 * np.sin(xs / 7)).astype(np.float32)
    ridge2 = (horizon - 6 - 14 * np.abs(np.sin(xs / 45 + 1.3)) - 5 * np.sin(xs / 11)).astype(np.float32)
    rays = value_noise(3, 120, 2, W, 1)[0]
    frames = []
    font, fdate = F(fname, fsize), F("Rajdhani-Bold.ttf", 22)
    for i in range(n):
        t = i / N * 2 * math.pi
        img = sky.copy()
        tw = 0.6 + 0.4 * np.sin(t * 2 + np.arange(len(sx)))
        img[sy, sx] += (stars[sy, sx] * tw)[:, None] * 230
        aur = np.zeros((H, W, 3), np.float32)
        for k, (yb0, amp, h0, ph, gain) in enumerate([(150, 26, 95, 0.0, 1.0), (118, 18, 70, 2.1, 0.6)]):
            base = yb0 + amp * np.sin(xs / 85 + t + ph) + 10 * np.sin(xs / 31 - 1.7 * t + ph) + 4 * np.sin(xs / 9 + 3 * t)
            hgt = h0 + 30 * np.sin(xs / 57 - t + ph)
            ray = 0.55 + 0.45 * np.sin(xs * 0.35 + 6 * np.roll(rays, int(i * (2 + k)))) * np.sin(xs * 0.07 - t * 2 + ph)
            ray = np.clip(ray, 0.1, 1) * (0.7 + 0.3 * np.sin(xs / 40 + t * 2 + k))
            up = (base[None, :] - YS) / hgt[None, :]
            inten = np.where(up >= 0, np.exp(-up * 2.2), np.exp(up * 14)) * ray[None, :] * gain
            fr = np.clip(up, 0, 1)[..., None]
            col = np.array([40, 255, 130]) * (1 - fr) + np.array([170, 50, 255]) * fr
            aur += col * inten[..., None]
        aur *= (YS < horizon)[..., None]
        img += aur * 0.95
        img += np.asarray(finish(aur).filter(ImageFilter.GaussianBlur(12)), np.float32) * 0.6
        # gory
        img[YS > ridge[None, :]] = np.array([4, 6, 14])
        img[YS > ridge2[None, :]] = np.array([2, 3, 8])
        # jezioro: odbicie z falowaniem
        for y in range(horizon, H):
            d = y - horizon
            src = horizon - 1 - d
            if src < 0:
                break
            shift = int(round(1.2 * math.sin(y * 0.45 + t * 3) * (0.2 + d / 80)))
            img[y] = np.roll(img[src], shift, axis=0) * (0.55 - d / 400)
        img[horizon] += np.array([10, 30, 30])
        im = finish(img)
        tl = Image.new("RGB", (W, H), 0)
        big_text(ImageDraw.Draw(tl), (240, 64), TIME, font, tcol, (0, 0, 0), 0)
        sh = np.asarray(tl.filter(ImageFilter.GaussianBlur(5)), np.float32)[..., :1] / 255
        im = finish(np.asarray(im, np.float32) * (1 - sh * 0.7) + np.asarray(tl, np.float32) + (glow(tl, 8, 1.0) - np.asarray(tl, np.float32)) * tglow)
        big_text(ImageDraw.Draw(im), (240, 300), DATE, fdate, tuple(int(c * 0.85) for c in tcol), (0, 10, 10), 2)
        frames.append(im)
    return frames


# ---------------------------------------------------------------- G. Ognisko
FIRE_PAL = np.array([[0, 0, 0], [40, 4, 0], [110, 14, 0], [190, 40, 0], [235, 90, 10], [255, 150, 30],
                     [255, 210, 80], [255, 245, 170], [255, 255, 240]], np.float32)


def fire_palette(v):
    v = np.clip(v, 0, 1) * (len(FIRE_PAL) - 1)
    i0 = np.floor(v).astype(int)
    i1 = np.minimum(i0 + 1, len(FIRE_PAL) - 1)
    f = (v - i0)[..., None]
    return FIRE_PAL[i0] * (1 - f) + FIRE_PAL[i1] * f


def campfire_frames():
    rng = np.random.default_rng(11)
    sky = np.zeros((H, W, 3), np.float32)
    f = (YS / H)[..., None]
    sky += np.array([2, 3, 12]) * (1 - f) + np.array([10, 8, 20]) * f
    # Droga Mleczna: ukosny pas szumu + gesciej gwiazd
    band = np.exp(-(((YS - 0.42 * XS) - 10) / 55) ** 2)
    neb = value_noise(4, 24, 16) * value_noise(9, 60, 40)
    sky += (band * neb)[..., None] * np.array([70, 60, 90])
    stars, sx, sy = star_layer(12, 320)
    sky += (stars * (0.4 + band))[..., None] * 230
    # swierki
    trees = Image.new("L", (W, H), 0)
    td = ImageDraw.Draw(trees)
    for x0, hgt in [(-10, 190), (30, 150), (70, 120), (395, 130), (430, 175), (470, 145), (120, 70), (350, 80)]:
        for k in range(7):
            w_ = 12 + k * (hgt / 18)
            y_ = 285 - hgt + k * hgt / 7
            td.polygon([(x0, y_), (x0 - w_, y_ + hgt / 5), (x0 + w_, y_ + hgt / 5)], fill=255)
    td.rectangle([0, 282, W, H], fill=255)
    tmask = np.asarray(trees, np.float32)[..., None] / 255
    # ogien: symulacja w siatce 64x56 (x2.2)
    fw, fh, sc = 90, 90, 2.0
    heat = np.zeros((fh, fw), np.float32)
    sparks = []
    font, fdate = F("Rajdhani-Bold.ttf", 100), F("Rajdhani-Bold.ttf", 22)
    frames = []
    fx0, fy0 = int(240 - fw * sc / 2), int(282 - fh * sc + 6)
    for step in range(N + 40):
        src = np.exp(-((np.arange(fw) - fw / 2) / 15) ** 2) * (0.85 + 0.3 * rng.random(fw))
        heat[-1] = src
        heat[-2] = np.maximum(heat[-2], src * 0.95)
        shift = rng.integers(-1, 2, (fh - 2, fw))
        idx = np.clip(np.arange(fw)[None, :] + shift, 0, fw - 1)
        below = heat[1:-1][np.arange(fh - 2)[:, None], idx]
        below = (np.roll(below, 1, 1) + below * 2 + np.roll(below, -1, 1)) / 4
        heat[:-2] = np.maximum(0, below - rng.random((fh - 2, fw)) * 0.032 - 0.004)
        # iskry
        if rng.random() < 0.7:
            sparks.append([240 + rng.normal(0, 14), 240.0, rng.normal(0, 0.6), -2.5 - rng.random() * 2.5, 1.0])
        for s in sparks:
            s[0] += s[2] + math.sin(s[1] * 0.08) * 0.6
            s[1] += s[3]
            s[4] -= 0.025
        sparks = [s for s in sparks if s[4] > 0 and s[1] > 0]
        if step < 40:
            continue
        i = step - 40
        img = sky.copy()
        flick = 0.85 + 0.15 * math.sin(i * 1.7) * math.sin(i * 0.6)
        # poswiata na ziemi i drzewach
        gl = np.exp(-(((XS - 240) / 170) ** 2 + ((YS - 285) / 70) ** 2)) * flick
        img = img * (1 - tmask) + tmask * (np.array([3, 3, 6]) + gl[..., None] * np.array([120, 50, 10]))
        fire = Image.fromarray(np.clip(fire_palette(heat ** 0.9) , 0, 255).astype(np.uint8)).resize(
            (int(fw * sc), int(fh * sc)), Image.BICUBIC).filter(ImageFilter.GaussianBlur(1.2))
        layer = Image.new("RGB", (W, H), 0)
        layer.paste(fire, (fx0, fy0))
        ld = ImageDraw.Draw(layer)
        for s in sparks:
            b = s[4]
            ld.point((s[0], s[1]), (255, int(200 * b), int(80 * b)))
            ld.point((s[0], s[1] + 1), (int(200 * b), int(90 * b), 0))
        img += glow(layer, 10, 0.8)
        im = finish(img)
        d = ImageDraw.Draw(im)
        # polana
        d.line([(190, 290), (290, 276)], fill=(60, 30, 14), width=10)
        d.line([(195, 276), (292, 291)], fill=(48, 24, 10), width=10)
        d.line([(195, 274), (288, 272)], fill=(255, 120, 30), width=1)
        tl = Image.new("RGB", (W, H), 0)
        big_text(ImageDraw.Draw(tl), (240, 66), TIME, font, (255, 236, 205), (0, 0, 0), 0)
        im = finish(np.asarray(im, np.float32) + glow(tl, 7, 0.4) * np.array([1, 0.75, 0.5]))
        big_text(ImageDraw.Draw(im), (240, 128), DATE, fdate, (255, 170, 90), (0, 0, 0), 0)
        frames.append(im)
    return frames


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    opts = [("E_czarna_dziura", blackhole_frames), ("F_zorza", aurora_frames), ("G_ognisko", campfire_frames)]
    sheet = Image.new("RGB", (W * 3 + 20, H), (40, 40, 40))
    for n, (name, fn) in enumerate(opts):
        frames = fn()
        frames[0].save(OUT / f"{name}.gif", save_all=True, append_images=frames[1:], duration=90, loop=0)
        sheet.paste(frames[N // 2], (n * (W + 10), 0))
        print(name)
    sheet.save(OUT / "sheet2.png")


def aurora_variants():
    sheet = Image.new("RGB", (W * 3 + 20, H), (40, 40, 40))
    for k, st in enumerate(AURORA_STYLES):
        frames = aurora_frames(st)
        frames[0].save(OUT / f"F_zorza_{st}.gif", save_all=True, append_images=frames[1:], duration=90, loop=0)
        sheet.paste(frames[N // 2], (k * (W + 10), 0))
    sheet.save(OUT / "F_zorza_warianty.png")


if __name__ == "__main__":
    import sys
    aurora_variants() if "zorza" in sys.argv else main()
