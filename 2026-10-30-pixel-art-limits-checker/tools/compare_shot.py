#!/usr/bin/env python3
"""Compare an emulator screenshot (PPM) with the source PNG snapped to the Mega Drive's 3 bits per channel.

Both sides are reduced to the 3-bit level per channel (nearest of round(k*255/7)); the emulator's own RGB output
for each level does not have to match those values. Prints the number of pixels that differ. MIT license."""
import sys

import numpy as np
from PIL import Image

src, shot, out = sys.argv[1:4]
a = np.array(Image.open(src).convert("RGB")).astype(int)
b_img = Image.open(shot).convert("RGB")
b_img.save(out)
b = np.array(b_img).astype(int)
grid = np.array([round(k * 255 / 7) for k in range(8)])


def level(x):
    return np.abs(x[..., None] - grid).argmin(axis=-1)


h, w = a.shape[:2]
print(f"source {w}x{h}, screenshot {b.shape[1]}x{b.shape[0]}")
b = b[:h, :w]
# map each emulator output value to the level it stands for: the emulator's distinct values per channel, sorted
levels_seen = sorted(set(np.unique(b)))
diff = (level(a) != level(b)).any(axis=2)
print(f"pixels that differ: {int(diff.sum())} of {h * w}")
print(f"emulator channel values seen: {levels_seen}")
