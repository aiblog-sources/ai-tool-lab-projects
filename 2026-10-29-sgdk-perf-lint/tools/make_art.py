"""Writes the two indexed PNGs the benchmark ROM uses (art drawn as code, MIT).

bench/res/ball.png  16x16, a shaded ball, for the sprite cases
bench/res/panel.png 128x64, a patterned panel (16x8 tiles), for VDP_drawImageEx
Needs Pillow.
"""
import math
import sys
from pathlib import Path

from PIL import Image

PAL = [
    (0, 0, 0), (34, 34, 68), (68, 68, 136), (102, 102, 204), (136, 136, 238),
    (204, 68, 68), (238, 136, 68), (238, 204, 102), (68, 136, 68), (102, 204, 102),
    (238, 238, 238), (170, 170, 170), (102, 102, 102), (68, 34, 34), (136, 68, 136), (204, 136, 204),
]


def indexed(w, h, pixel):
    img = Image.new("P", (w, h))
    flat = [c for rgb in PAL for c in rgb]
    img.putpalette(flat + [0] * (768 - len(flat)))
    img.putdata([pixel(x, y) for y in range(h) for x in range(w)])
    return img


def ball(x, y):
    d = math.hypot(x - 7.5, y - 7.5)
    if d > 7.5:
        return 0
    light = math.hypot(x - 5, y - 5)
    return 7 if light < 2 else 6 if light < 5 else 5 if d < 6.5 else 13


def panel(x, y):
    # bands, rings and a grid, so most tiles differ and compression has work to do
    r = int(math.hypot(x - 40, y - 30)) // 6
    band = (x // 8 + y // 8) % 3
    if x % 32 == 0 or y % 16 == 0:
        return 10
    return [1, 2, 3, 4, 8, 9, 14, 15][(r + band) % 8]


if __name__ == "__main__":
    out = Path(sys.argv[1] if len(sys.argv) > 1 else "bench/res")
    indexed(16, 16, ball).save(out / "ball.png")
    indexed(128, 64, panel).save(out / "panel.png")
    print("wrote", out / "ball.png", out / "panel.png")
