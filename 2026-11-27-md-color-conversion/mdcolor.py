#!/usr/bin/env python3
"""mdcolor: convert SNES (or any 24-bit) colors to Mega Drive 9-bit colors, and measure what each method loses.

Usage:
  mdcolor.py convert in.png out.png [--method lab] [--display linear|table]   write the image in Mega Drive colors
  mdcolor.py report in.png [more.png ...] [--json f]                         compare every method on the images
  mdcolor.py all-snes [--json f]                                              every one of the 32,768 SNES colors

Methods (each picks one of 8 levels per channel, 512 colors in all):
  trunc     keep the top 3 bits of each 8-bit channel (v >> 5). This is what SGDK's rescomp does.
  macro     SGDK's RGB24_TO_VDPCOLOR macro (for code, not rescomp): adds 16, then keeps the top 3 bits
  round     nearest level per channel on an evenly spaced grid (k * 255 / 7)
  round-table  nearest level per channel on the non-linear table below
  lab       nearest of the 512 colors in CIELAB by CIEDE2000, for the chosen display model
  distinct  like lab, but two different source colors never end up as the same Mega Drive color
            (minimum total CIEDE2000 under that rule, solved as an assignment problem)

Display models (what RGB a level turns into on screen):
  linear    0, 36, 73, 109, 146, 182, 219, 255 (evenly spaced; what most emulators and tools assume)
  table     0, 52, 87, 116, 144, 172, 206, 255 (non-linear levels credited to hardware measurements by TmEE and Eke;
            not verified here)

SNES colors are expanded from 5 to 8 bits as (c << 3) | (c >> 2). MIT license."""
import argparse
import json
import sys

import numpy as np
from PIL import Image

DISPLAYS = {
    "linear": np.array([round(k * 255 / 7) for k in range(8)], dtype=float),
    "table": np.array([0, 52, 87, 116, 144, 172, 206, 255], dtype=float),
}
METHODS = ["trunc", "macro", "round", "round-table", "lab", "distinct"]


# ---------- color science (sRGB D65 -> CIELAB, CIEDE2000) ----------

def srgb_to_lab(rgb):
    c = np.asarray(rgb, dtype=float) / 255.0
    c = np.where(c <= 0.04045, c / 12.92, ((c + 0.055) / 1.055) ** 2.4)
    m = np.array([[0.4124564, 0.3575761, 0.1804375],
                  [0.2126729, 0.7151522, 0.0721750],
                  [0.0193339, 0.1191920, 0.9503041]])
    xyz = c @ m.T / np.array([0.95047, 1.0, 1.08883])
    f = np.where(xyz > (6 / 29) ** 3, np.cbrt(xyz), xyz / (3 * (6 / 29) ** 2) + 4 / 29)
    return np.stack([116 * f[..., 1] - 16, 500 * (f[..., 0] - f[..., 1]), 200 * (f[..., 1] - f[..., 2])], axis=-1)


def ciede2000(lab1, lab2):
    L1, a1, b1 = np.moveaxis(np.asarray(lab1, dtype=float), -1, 0)
    L2, a2, b2 = np.moveaxis(np.asarray(lab2, dtype=float), -1, 0)
    C1, C2 = np.hypot(a1, b1), np.hypot(a2, b2)
    Cb = (C1 + C2) / 2
    G = 0.5 * (1 - np.sqrt(Cb ** 7 / (Cb ** 7 + 25.0 ** 7)))
    a1p, a2p = (1 + G) * a1, (1 + G) * a2
    C1p, C2p = np.hypot(a1p, b1), np.hypot(a2p, b2)
    h1p = np.degrees(np.arctan2(b1, a1p)) % 360
    h2p = np.degrees(np.arctan2(b2, a2p)) % 360
    dLp = L2 - L1
    dCp = C2p - C1p
    dh = h2p - h1p
    dh = np.where(C1p * C2p == 0, 0, np.where(dh > 180, dh - 360, np.where(dh < -180, dh + 360, dh)))
    dHp = 2 * np.sqrt(C1p * C2p) * np.sin(np.radians(dh / 2))
    Lbp = (L1 + L2) / 2
    Cbp = (C1p + C2p) / 2
    hsum = h1p + h2p
    hbp = np.where(C1p * C2p == 0, hsum,
                   np.where(np.abs(h1p - h2p) <= 180, hsum / 2, np.where(hsum < 360, (hsum + 360) / 2, (hsum - 360) / 2)))
    T = (1 - 0.17 * np.cos(np.radians(hbp - 30)) + 0.24 * np.cos(np.radians(2 * hbp))
         + 0.32 * np.cos(np.radians(3 * hbp + 6)) - 0.20 * np.cos(np.radians(4 * hbp - 63)))
    dtheta = 30 * np.exp(-(((hbp - 275) / 25) ** 2))
    Rc = 2 * np.sqrt(Cbp ** 7 / (Cbp ** 7 + 25.0 ** 7))
    Sl = 1 + 0.015 * (Lbp - 50) ** 2 / np.sqrt(20 + (Lbp - 50) ** 2)
    Sc = 1 + 0.045 * Cbp
    Sh = 1 + 0.015 * Cbp * T
    Rt = -np.sin(np.radians(2 * dtheta)) * Rc
    return np.sqrt((dLp / Sl) ** 2 + (dCp / Sc) ** 2 + (dHp / Sh) ** 2 + Rt * (dCp / Sc) * (dHp / Sh))


# ---------- the 512 Mega Drive colors ----------

ALL_LEVELS = np.array([(r, g, b) for r in range(8) for g in range(8) for b in range(8)])


def levels_to_rgb(levels, display):
    return DISPLAYS[display][np.asarray(levels)]


def cram_word(level):
    """Mega Drive CRAM word: 0000 BBB0 GGG0 RRR0."""
    r, g, b = (int(x) for x in level)
    return (b << 9) | (g << 5) | (r << 1)


def snes_expand(c5):
    c5 = np.asarray(c5)
    return (c5 << 3) | (c5 >> 2)


def snap_to_snes(rgb):
    """Nearest 5-bit SNES color for each 8-bit channel value, returned as expanded 8-bit."""
    grid = snes_expand(np.arange(32))
    rgb = np.asarray(rgb)
    return grid[np.abs(rgb[..., None].astype(int) - grid).argmin(axis=-1)]


def convert_colors(colors, method, display="linear"):
    """colors: (n, 3) array of distinct 8-bit RGB. Returns (n, 3) array of levels 0..7."""
    colors = np.asarray(colors, dtype=int)
    if method == "trunc":
        return colors >> 5
    if method == "macro":
        return np.minimum(colors + 16, 255) >> 5
    if method == "round":
        return np.rint(colors * 7 / 255).astype(int)
    if method == "round-table":
        return np.abs(colors[..., None] - DISPLAYS["table"]).argmin(axis=-1)
    lab_src = srgb_to_lab(colors)
    lab_md = srgb_to_lab(levels_to_rgb(ALL_LEVELS, display))
    cost = ciede2000(lab_src[:, None, :], lab_md[None, :, :])
    if method == "lab":
        return ALL_LEVELS[cost.argmin(axis=1)]
    if method == "distinct":
        from scipy.optimize import linear_sum_assignment
        rows, cols = linear_sum_assignment(cost)
        out = np.zeros_like(colors)
        out[rows] = ALL_LEVELS[cols]
        return out
    raise ValueError(method)


def load_rgb(path):
    img = Image.open(path).convert("RGBA")
    a = np.array(img)
    return a[:, :, :3].astype(int), a[:, :, 3] >= 128


def measure(rgb, opaque, method, display_convert, display_view):
    """Convert with one display model in mind, judge the result on screen under another."""
    px = rgb[opaque]
    colors, inverse, counts = np.unique(px, axis=0, return_inverse=True, return_counts=True)
    inverse = inverse.reshape(-1)
    lv = convert_colors(colors, method, display_convert)
    src_lab = srgb_to_lab(colors)
    out_lab = srgb_to_lab(levels_to_rgb(lv, display_view))
    de = ciede2000(src_lab, out_lab)
    w = counts / counts.sum()
    per_pixel = de[inverse]
    out_keys = [tuple(x) for x in lv]
    return dict(
        method=method, convert_for=display_convert, viewed_on=display_view,
        source_colors=int(len(colors)), md_colors=int(len(set(out_keys))),
        merged=int(len(colors) - len(set(out_keys))),
        mean_de=round(float((de * w).sum()), 2), p95_de=round(float(np.percentile(per_pixel, 95)), 2),
        max_de=round(float(de.max()), 2),
        mean_dL=round(float(((out_lab[:, 0] - src_lab[:, 0]) * w).sum()), 2),
    )


def cmd_convert(a):
    rgb, opaque = load_rgb(a.input)
    if a.snap_snes:
        rgb = snap_to_snes(rgb)
    img = Image.open(a.input).convert("RGBA")
    arr = np.array(img)
    px = rgb[opaque]
    colors, inverse = np.unique(px, axis=0, return_inverse=True)
    lv = convert_colors(colors, a.method, a.display)
    out_rgb = levels_to_rgb(lv, "linear").astype(np.uint8)[inverse.reshape(-1)]
    arr[opaque, :3] = out_rgb
    Image.fromarray(arr).save(a.output)
    merged = len(colors) - len({tuple(x) for x in lv})
    print(f"{a.input}: {len(colors)} colors -> {len(colors) - merged} Mega Drive colors ({merged} merged), "
          f"method {a.method}, display {a.display}; written in linear RGB (k*255/7) so rescomp keeps the levels")


def cmd_report(a):
    rows = []
    for path in a.inputs:
        rgb, opaque = load_rgb(path)
        if a.snap_snes:
            rgb = snap_to_snes(rgb)
        for view in ("linear", "table"):
            for m in METHODS:
                r = measure(rgb, opaque, m, view, view)
                r["file"] = path
                rows.append(r)
    for r in rows:
        print(f"{r['file']}: {r['method']:8s} on {r['viewed_on']:6s} mean dE {r['mean_de']:5.2f} p95 {r['p95_de']:5.2f} "
              f"max {r['max_de']:5.2f} dL {r['mean_dL']:+5.2f} merged {r['merged']}/{r['source_colors']}")
    if a.json:
        json.dump(rows, open(a.json, "w"), indent=1)


def cmd_all_snes(a):
    c5 = np.array([(r, g, b) for r in range(32) for g in range(32) for b in range(32)])
    colors = snes_expand(c5)
    src_lab = srgb_to_lab(colors)
    rows = []
    for view in ("linear", "table"):
        for m in ["trunc", "macro", "round", "round-table", "lab"]:
            for conv in (["linear", "table"] if m == "lab" else ["linear"]):
                lv = convert_colors(colors, m, conv)
                de = ciede2000(src_lab, srgb_to_lab(levels_to_rgb(lv, view)))
                dl = srgb_to_lab(levels_to_rgb(lv, view))[:, 0] - src_lab[:, 0]
                rows.append(dict(method=m, convert_for=conv if m == "lab" else "-", viewed_on=view,
                                 mean_de=round(float(de.mean()), 2), median_de=round(float(np.median(de)), 2),
                                 p95_de=round(float(np.percentile(de, 95)), 2), max_de=round(float(de.max()), 2),
                                 mean_dL=round(float(dl.mean()), 2),
                                 over_5=int((de > 5).sum())))
    for r in rows:
        print(f"{r['method']:5s} conv {r['convert_for']:6s} view {r['viewed_on']:6s} mean {r['mean_de']:5.2f} "
              f"median {r['median_de']:5.2f} p95 {r['p95_de']:5.2f} max {r['max_de']:5.2f} dL {r['mean_dL']:+5.2f} "
              f">5: {r['over_5']}")
    if a.json:
        json.dump(rows, open(a.json, "w"), indent=1)


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    c = sub.add_parser("convert")
    c.add_argument("input"); c.add_argument("output")
    c.add_argument("--method", choices=METHODS, default="lab")
    c.add_argument("--display", choices=list(DISPLAYS), default="linear")
    c.add_argument("--snap-snes", action="store_true", help="snap source colors to the SNES 5-bit grid first")
    r = sub.add_parser("report")
    r.add_argument("inputs", nargs="+")
    r.add_argument("--snap-snes", action="store_true", help="snap source colors to the SNES 5-bit grid first")
    r.add_argument("--json")
    s = sub.add_parser("all-snes")
    s.add_argument("--json")
    a = ap.parse_args(argv)
    {"convert": cmd_convert, "report": cmd_report, "all-snes": cmd_all_snes}[a.cmd](a)


if __name__ == "__main__":
    main()
