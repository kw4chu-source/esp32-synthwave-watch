"""Zasoby tarczy "blackhole": mapy pikseli dysku akrecyjnego.

Dla kazdego piksela dwoch prostokatow (D = dysk bezposredni, L = obraz
soczewkowany wokol cienia) zapisane: indeks promienia (0..127), kat (0..255),
jasnosc (profil x Doppler) i statyczna poswiata. Na ESP32 tekstura
tex[promien][kat + obrot[promien]] - kazdy pierscien kreci sie z wlasna
predkoscia (rotacja roznicowa). Podglad: out/preview.png (ta sama matematyka).

  python tools/prerender/blackhole/gen_assets.py
"""

from __future__ import annotations

import sys
from pathlib import Path

import numpy as np
from PIL import Image, ImageFilter

HERE = Path(__file__).parent
sys.path.insert(0, str(HERE.parent))
import common_hd as hd  # noqa: E402

ROOT = HERE.parents[2]
OUT_SRC = ROOT / "src" / "faces" / "blackhole"
W, H = hd.W, hd.H

CX, CY, RS = 240, 106, 36.0
RIN, ROUT, INC = 54.0, 225.0, 0.14
NR = 128
NONE, SHADOW = 255, 254
D_RECT = (12, CY - 40, 456, 81)            # x, y, w, h
L_RECT = (CX - 94, CY - 94, 188, 189)


def r_index(R):
    return np.clip((R - RIN) / (ROUT - RIN) * NR, 0, NR - 1).astype(np.uint8)


def theta8(th):
    return (np.round(th / (2 * np.pi) * 256) % 256).astype(np.uint8)


def profile(R, th):
    prof = np.exp(-(R - RIN) / 55) * np.clip((R - RIN) / 6, 0, 1) * (R < ROUT) * (R >= RIN)
    dop = (1 - 0.55 * np.cos(th)) / 1.55
    return prof * dop


def full_maps():
    ys, xs = np.mgrid[0:H, 0:W].astype(np.float32)
    dx, dy = xs + 0.5 - CX, ys + 0.5 - CY
    rho, psi = np.hypot(dx, dy), np.arctan2(dy, dx)
    R, th = np.hypot(dx, dy / INC), np.arctan2(dy / INC, dx)
    # dysk bezposredni
    wd = profile(R, th)
    d_on = wd > 0.004
    # obraz soczewkowany: pierscien wokol cienia = dalsza czesc dysku
    band = (rho > RS * 1.06) & (rho < RS * 2.5)
    Rl = RIN + (rho - RS * 1.06) / (RS * 1.44) * (ROUT * 0.5 - RIN)
    lw = np.where(dy < 0, 1.0, 0.45) * np.clip((RS * 2.5 - rho) / (RS * 0.6), 0, 1) * 0.8
    wl = profile(Rl, psi) ** 0.6 * lw * band
    l_on = wl > 0.004
    shadow = rho <= RS
    # statyczna poswiata: rozmyty sredni obraz dysku + pierscien fotonowy
    mean = np.zeros((H, W), np.float32)
    mean += np.where(d_on, wd, 0) + np.where(l_on, wl, 0)
    mean *= ~shadow | (dy > 0)
    haze = np.asarray(Image.fromarray(np.clip(mean * 255, 0, 255).astype(np.uint8)).filter(
        ImageFilter.GaussianBlur(7)), np.float32) / 255 * 0.9
    ring = np.exp(-((rho - RS * 1.03) / 1.4) ** 2) * 0.95
    haze = np.clip(haze + ring, 0, 1) * ~shadow
    return dict(dy=dy, R=R, th=th, wd=wd, d_on=d_on, Rl=Rl, psi=psi, wl=wl, l_on=l_on, shadow=shadow, haze=haze)


def texture():
    rng = np.random.default_rng(3)
    a = np.arange(256) / 256 * 2 * np.pi
    R = RIN + (np.arange(NR) + 0.5) / NR * (ROUT - RIN)
    A, RR = np.meshgrid(a, R)
    tex = 0.55 + 0.25 * np.sin(7 * A + RR * 0.09) + 0.15 * np.sin(19 * A - RR * 0.21) + 0.1 * np.sin(3 * A + RR * 0.04)
    # drobna turbulencja wzdluz orbity
    noise = np.asarray(Image.fromarray((rng.random((NR // 4, 64)) * 255).astype(np.uint8)).resize(
        (256, NR), Image.BICUBIC), np.float32) / 255
    tex = tex * (0.75 + 0.5 * noise)
    return (np.clip(tex / 1.4, 0, 1) * 255).astype(np.uint8)


def temp_ramp():
    """[16 pasm temperatury][256 jasnosci] RGB888."""
    out = np.zeros((16, 256, 3), np.uint8)
    for b in range(16):
        hot = np.clip(1 - (b + 0.5) / 16 * (ROUT - RIN) / 120, 0, 1)
        col = np.array([255, 90, 20]) * (1 - hot) + np.array([255, 235, 190]) * hot
        for i in range(256):
            out[b, i] = np.clip(col * (i / 255) ** 0.65 * 1.7, 0, 255)
    return out


def haze_lut():
    out = []
    for i in range(256):
        f = i / 255
        c = np.array([255, 120, 40]) * min(1, f * 1.6) + np.array([0, 110, 140]) * max(0, f - 0.55) / 0.45
        out.append(tuple(int(min(255, v * f ** 0.5)) for v in c))
    return out


def pack(m, rect, use_d):
    x, y, w, h = rect
    sl = (slice(y, y + h), slice(x, x + w))
    if use_d:
        on = m["d_on"][sl]
        r = np.where(on, r_index(m["R"][sl]), NONE)
        t = theta8(m["th"][sl])
        wv = (np.clip(m["wd"][sl], 0, 1) * 255).astype(np.uint8)
        hz = m["haze"][sl].copy()
        lx, ly, lw_, lh = L_RECT
        ys, xs = np.mgrid[y:y + h, x:x + w]
        hz[(xs >= lx) & (xs < lx + lw_) & (ys >= ly) & (ys < ly + lh)] = 0  # poswiata liczona w L
    else:
        on = m["l_on"][sl]
        r = np.where(m["shadow"][sl], SHADOW, np.where(on, r_index(m["Rl"][sl]), NONE))
        t = theta8(m["psi"][sl])
        wv = (np.clip(m["wl"][sl], 0, 1) * 255).astype(np.uint8)
        hz = m["haze"][sl]
    hz8 = (np.clip(hz, 0, 1) * 255).astype(np.uint8)
    return np.stack([r.astype(np.uint8), t, wv, hz8], -1)


def preview(dmap, lmap, tex, ramp, hz, stars, clock, date):
    """Ta sama skladanka co compose() na ESP32 (bez ditheringu)."""
    img = np.zeros((H, W, 3), np.float32)
    for x, y, b in stars:
        img[y, x] = b
    def add(rect, m, shadow_mask=None, front_only=False):
        x, y, w, h = rect
        r, t, wv, h8 = [m[..., k].astype(int) for k in range(4)]
        on = r < NR
        ys = np.arange(y, y + h)[:, None]
        if shadow_mask is not None:
            on &= (ys > CY) | ~shadow_mask
        I = tex[np.clip(r, 0, NR - 1), t] * wv // 255
        col = ramp[np.clip(r, 0, NR - 1) >> 3, I]
        img[y:y + h, x:x + w] += np.where(on[..., None], col, 0) + np.array(hz)[h8] * (h8 > 0)[..., None]
    lx, ly, lw, lh = L_RECT
    add(L_RECT, lmap)
    img[ly:ly + lh, lx:lx + lw] *= (lmap[..., 0] != SHADOW)[..., None]
    sh = np.zeros((D_RECT[3], D_RECT[2]), bool)
    ys, xs = np.mgrid[D_RECT[1]:D_RECT[1] + D_RECT[3], D_RECT[0]:D_RECT[0] + D_RECT[2]]
    sh = np.hypot(xs + 0.5 - CX, ys + 0.5 - CY) <= RS
    add(D_RECT, dmap, shadow_mask=sh)
    return Image.fromarray(np.clip(img, 0, 255).astype(np.uint8))


def main():
    m = full_maps()
    dmap, lmap = pack(m, D_RECT, True), pack(m, L_RECT, False)
    tex, ramp, hz = texture(), temp_ramp(), haze_lut()
    rng = np.random.default_rng(5)
    stars = []
    while len(stars) < 160:
        x, y = int(rng.integers(0, W)), int(rng.integers(0, H))
        if np.hypot(x - CX, y - CY) > RS * 2.7 and not (abs(y - CY) < 34 and abs(x - CX) < 230):
            stars.append((x, y, int(60 + rng.random() ** 2 * 190)))

    warm = hd.neon_lut((255, 170, 90), (255, 244, 225), ((8, 0.6), (3, 0.9)))
    datel = hd.neon_lut((255, 150, 60), (255, 200, 150), ((3, 0.5),))
    clock = hd.build_clock(hd.font("Rajdhani-Bold.ttf", 104), "neon", cy=250, radii=((8, 0.6), (3, 0.9)))
    date = hd.build_date(hd.font("Rajdhani-Bold.ttf", 22), "neon", baseline=309, radii=((3, 0.5),))

    extra_h = f"""
constexpr int CX = {CX}, CY = {CY};
constexpr int NR = {NR};
constexpr uint8_t NONE = {NONE}, SHADOW = {SHADOW};
constexpr int D_X = {D_RECT[0]}, D_Y = {D_RECT[1]}, D_W = {D_RECT[2]}, D_H = {D_RECT[3]};
constexpr int L_X = {L_RECT[0]}, L_Y = {L_RECT[1]}, L_W = {L_RECT[2]}, L_H = {L_RECT[3]};
// [promien | NONE | SHADOW, kat, jasnosc, poswiata]
extern const uint8_t D_MAP[D_W * D_H][4];
extern const uint8_t L_MAP[L_W * L_H][4];
extern const uint8_t TEX[NR][256];
extern const uint8_t RAMP[16][256][3];
extern const uint8_t HAZE[256][3];
extern const uint8_t LUT_CLOCK[256][3];
extern const uint8_t LUT_DATE[256][3];
struct Star {{ int16_t x, y; uint8_t b; }};
constexpr int STAR_COUNT = {len(stars)};
extern const Star STARS[STAR_COUNT];
"""
    extra_cpp = hd.c_array("D_MAP", dmap, dims="[D_W * D_H][4]")
    extra_cpp += hd.c_array("L_MAP", lmap, dims="[L_W * L_H][4]")
    extra_cpp += hd.c_array("TEX", tex, dims="[NR][256]")
    extra_cpp += hd.c_array("RAMP", ramp, dims="[16][256][3]")
    extra_cpp += hd.lut_array("HAZE", hz)
    extra_cpp += hd.lut_array("LUT_CLOCK", warm)
    extra_cpp += hd.lut_array("LUT_DATE", datel)
    extra_cpp += "const Star STARS[STAR_COUNT] = {" + ", ".join(f"{{{x}, {y}, {b}}}" for x, y, b in stars) + "};\n"
    hd.write_assets(OUT_SRC, "tools/prerender/blackhole/gen_assets.py", clock, date, extra_h, extra_cpp)
    out = HERE / "out"
    out.mkdir(exist_ok=True)
    preview(dmap, lmap, tex, ramp, hz, stars, clock, date).save(out / "preview.png")
    print(f"D {D_RECT} L {L_RECT}; cyfry top {clock['top']} h {clock['cell_h']}; data {date['top']}..{date['bottom']}")


if __name__ == "__main__":
    main()
