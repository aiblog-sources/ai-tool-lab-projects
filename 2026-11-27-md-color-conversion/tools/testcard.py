#!/usr/bin/env python3
"""Test card of four 15-shade SNES ramps (grey, sky, skin, foliage), one ramp per Mega Drive palette.

Writes an indexed PNG (320x224, index = palette * 16 + shade, color 0 of each palette = black backdrop), so SGDK
rescomp can take it as-is. With --method it writes the same card converted by mdcolor (levels written as k*255/7,
which rescomp's truncation keeps). Usage: testcard.py out.png [--method trunc|round|lab|distinct]. MIT license."""
import argparse
import sys
from pathlib import Path

import numpy as np
from PIL import Image

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
import mdcolor as m  # noqa: E402


def ramp(a, b, n=15):
    a, b = np.array(a, float), np.array(b, float)
    return [tuple(int(round(x)) for x in a + (b - a) * i / (n - 1)) for i in range(n)]


# 5-bit SNES endpoints (0..31 per channel)
RAMPS = {
    "grey": ramp((1, 1, 1), (31, 31, 31)),
    "sky": ramp((2, 4, 12), (22, 27, 31)),
    "skin": ramp((8, 3, 2), (31, 26, 21)),
    "foliage": ramp((1, 5, 2), (20, 30, 8)),
}


def snes_palettes():
    return [[tuple(int(v) for v in m.snes_expand(np.array(c))) for c in r] for r in RAMPS.values()]


def card(palettes):
    idx = np.zeros((224, 320), dtype=np.uint8)
    for p in range(4):
        for s in range(15):
            y, x = 16 + p * 48, 20 + s * 19
            idx[y:y + 40, x:x + 16] = p * 16 + 1 + s
    pal = []
    for p in range(4):
        pal += [(0, 0, 0)] + palettes[p]
    img = Image.fromarray(idx, mode="P")
    img.putpalette([v for c in pal for v in c])
    return img


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("out")
    ap.add_argument("--method", choices=m.METHODS)
    ap.add_argument("--display", default="linear", choices=list(m.DISPLAYS))
    a = ap.parse_args()
    pals = snes_palettes()
    if a.method:
        # distinct is solved per palette (a merge only hurts inside one ramp); the others are per color anyway
        out = []
        for p in pals:
            lv = m.convert_colors(np.array(p), a.method, a.display)
            out.append([tuple(int(v) for v in m.levels_to_rgb(l, "linear")) for l in lv])
        pals = out
    card(pals).save(a.out)


if __name__ == "__main__":
    main()
