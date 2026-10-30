#!/usr/bin/env python3
"""
NES-style background: sunset over mountains and a lake.
256x240 (full NES screen), saved as art.png.

The art follows NES background rules:
  * All colours come from the NES master palette.
  * 4 background palettes of 4 colours that share one backdrop colour (0x0F).
  * Every 16x16 attribute area uses exactly one of those palettes.
The script checks the attribute rule and prints the number of unique 8x8 tiles.
"""
from PIL import Image
import numpy as np

W, H = 256, 240

# NES master palette (2C02-style RGB values) for the colours used
NES = {
    0x0F: (0x00, 0x00, 0x00),
    0x03: (0x44, 0x28, 0xBC),
    0x04: (0x94, 0x00, 0x84),
    0x15: (0xE4, 0x00, 0x58),
    0x27: (0xFC, 0xA0, 0x44),
    0x38: (0xF8, 0xD8, 0x78),
}

# Background palettes (index 0 is the shared backdrop colour)
PALETTES = [
    (0x0F, 0x03, 0x04, 0x15),  # P0 upper sky
    (0x0F, 0x15, 0x27, 0x38),  # P1 lower sky, sun, mountains
    (0x0F, 0x03, 0x27, 0x38),  # P2 lake
    (0x0F, 0x03, 0x04, 0x15),  # P3 spare (same as P0)
]

HORIZON = 144  # lake starts here (multiple of 16, on an attribute boundary)

BAYER = np.array([[0, 8, 2, 10],
                  [12, 4, 14, 6],
                  [3, 11, 1, 9],
                  [15, 7, 13, 5]])


def dith(a, b, t, x, y):
    """Ordered dither from colour a (t=0) to colour b (t=1)."""
    return b if BAYER[y & 3][x & 3] < t * 16 else a


img = np.zeros((H, W), dtype=int)

# ---------------------------------------------------------------- sky
# gradient stops: (y, colour); between stops we dither
stops = [(0, 0x03), (14, 0x03), (30, 0x04), (40, 0x04), (48, 0x15),
         (58, 0x15), (92, 0x27), (112, 0x27), (140, 0x38), (HORIZON, 0x38)]


def sky_color(x, y):
    for (y0, c0), (y1, c1) in zip(stops, stops[1:]):
        if y0 <= y < y1:
            if c0 == c1:
                return c0
            return dith(c0, c1, (y - y0) / (y1 - y0), x, y)
    return stops[-1][1]


for y in range(HORIZON):
    for x in range(W):
        img[y, x] = sky_color(x, y)


# ---------------------------------------------------------------- clouds
def cloud(cx, cy, rx, ry, body, under, ymin, ymax):
    for y in range(max(ymin, cy - ry), min(ymax, cy + ry + 1)):
        for x in range(max(0, cx - rx), min(W, cx + rx + 1)):
            dx = (x - cx) / rx
            dy = (y - cy) / ry
            if dx * dx + dy * dy <= 1.0:
                img[y, x] = under if y >= cy + ry // 2 else body


# upper sky clouds (P0 colours: magenta / pink-red)
cloud(52, 22, 30, 3, 0x15, 0x04, 0, 48)
cloud(72, 27, 22, 2, 0x04, 0x04, 0, 48)
cloud(196, 16, 36, 3, 0x15, 0x04, 0, 48)
cloud(170, 36, 20, 2, 0x15, 0x15, 0, 48)
# lower sky streaks lit by the sun (P1 colours)
cloud(40, 70, 26, 2, 0x38, 0x27, 48, HORIZON)
cloud(214, 66, 30, 2, 0x38, 0x27, 48, HORIZON)
cloud(128, 84, 40, 1, 0x38, 0x38, 48, HORIZON)

# ---------------------------------------------------------------- sun
SX, SY, SR = 128, 120, 20
for y in range(SY - SR, SY + SR + 1):
    for x in range(SX - SR, SX + SR + 1):
        if (x - SX) ** 2 + (y - SY) ** 2 <= SR * SR + SR:
            d = y - SY
            # classic retro horizontal cut-lines in the lower half
            if d > 2 and (d % 5 == 0 or (d > 10 and d % 5 == 3)):
                img[y, x] = 0x27
            else:
                img[y, x] = 0x38

# ---------------------------------------------------------------- mountains
xs = np.arange(W)
far_pts = [(0, 104), (18, 90), (36, 100), (60, 80), (84, 98), (100, 92),
           (116, 112), (140, 106), (166, 84), (190, 98), (208, 88),
           (230, 104), (256, 94)]
near_pts = [(0, 114), (16, 104), (30, 118), (48, 108), (72, 128),
            (98, 138), (128, 141), (158, 137), (184, 124), (204, 110),
            (222, 118), (240, 102), (256, 110)]

far = np.interp(xs, [p[0] for p in far_pts], [p[1] for p in far_pts]).astype(int)
near = np.interp(xs, [p[0] for p in near_pts], [p[1] for p in near_pts]).astype(int)

for x in range(W):
    # far range: pink-red silhouette with a sun-facing rim light
    for y in range(far[x], HORIZON):
        img[y, x] = 0x15
    slope = far[min(x + 1, W - 1)] - far[max(x - 1, 0)]
    facing_sun = (x < SX and slope > 0) or (x > SX and slope < 0)
    if facing_sun:
        img[far[x], x] = 0x38
        if far[x] + 1 < HORIZON and abs(slope) > 2:
            img[far[x] + 1, x] = 0x27
    # near range: black silhouette
    for y in range(near[x], HORIZON):
        img[y, x] = 0x0F

# ---------------------------------------------------------------- lake
LAKE_H = H - HORIZON
for d in range(LAKE_H):
    y = HORIZON + d
    for x in range(W):
        c = 0x03
        # bottom of the lake darkens toward black
        if d > 60:
            c = dith(0x03, 0x0F, min(1.0, (d - 60) / 36), x, y)
        # reflection of the near mountains (squashed, with ripple gaps)
        refl = int((HORIZON - near[x]) * 0.7)
        if d < refl:
            gap = (d % 4 == 3) and ((x // 4 + d // 4) & 1)
            if not gap:
                c = 0x0F
        # sun reflection: shimmering horizontal streaks
        if d % 3 != 2:
            w = 26 - d * 0.22 + ((d * 7) % 5) - 2
            if w > 1:
                dx = abs(x - SX)
                if dx < w * 0.45:
                    c = 0x38
                elif dx < w:
                    c = 0x27
        # sparse ripple glints across the open water
        if d > 4 and d % 6 == 2 and c == 0x03:
            if (x + d * 5) % 32 < 3:
                c = 0x27
        img[y, x] = c

# ---------------------------------------------------------------- checks
ok = True
for by in range(0, H, 16):
    for bx in range(0, W, 16):
        used = set(np.unique(img[by:by + 16, bx:bx + 16]).tolist())
        if not any(used <= set(p) for p in PALETTES):
            ok = False
            print(f"attribute area ({bx},{by}) uses {sorted(hex(c) for c in used)}")
print("attribute/palette check:", "OK" if ok else "FAILED")

tiles = set()
for ty in range(0, H, 8):
    for tx in range(0, W, 8):
        tiles.add(img[ty:ty + 8, tx:tx + 8].tobytes())
print("unique 8x8 tiles:", len(tiles), "(NES limit 256 per pattern table)")

# ---------------------------------------------------------------- save
rgb = np.zeros((H, W, 3), dtype=np.uint8)
for code, col in NES.items():
    rgb[img == code] = col
Image.fromarray(rgb, "RGB").save("art.png")
print("saved art.png")
