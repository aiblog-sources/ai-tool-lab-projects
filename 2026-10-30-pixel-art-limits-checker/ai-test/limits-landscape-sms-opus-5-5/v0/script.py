#!/usr/bin/env python3
"""Sunset over mountains and a lake, as a Sega Master System background.

How it respects the hardware:
  * Size 256x192, the full SMS screen (32x24 tiles).
  * Every color is from the SMS master palette (2 bits per channel:
    0/85/170/255).
  * The image uses only 16 colors, so a single background palette covers it.
  * Unique 8x8 tiles, counting H/V flips as the same tile, are capped at 440.
    That fits the 448 tile slots left in VRAM after the name table and the
    sprite attribute table. If the drawing goes over, the rarest tiles are
    merged into their nearest kept tile.
Output: art.png (indexed / paletted PNG).
"""
import math
import numpy as np
from PIL import Image

W, H = 256, 192
TILE_LIMIT = 440

PAL = [
    (0, 0, 0),        # 0 black
    (0, 0, 85),       # 1 navy
    (85, 0, 85),      # 2 dark purple
    (85, 0, 170),     # 3 violet
    (170, 0, 85),     # 4 magenta
    (255, 0, 85),     # 5 pink red
    (255, 85, 0),     # 6 orange
    (255, 170, 0),    # 7 amber
    (255, 255, 85),   # 8 yellow
    (255, 255, 255),  # 9 white (sun core)
    (170, 85, 170),   # 10 far mountain lavender
    (85, 85, 170),    # 11 far mountain shade
    (0, 85, 85),      # 12 dark teal (shore/trees)
    (170, 85, 85),    # 13 rose rim light
    (0, 85, 170),     # 14 lake blue
    (255, 170, 85),   # 15 peach
]
for c in PAL:
    assert all(v in (0, 85, 170, 255) for v in c)

img = np.zeros((H, W), dtype=np.uint8)

BAYER = np.array([[0, 8, 2, 10],
                  [12, 4, 14, 6],
                  [3, 11, 1, 9],
                  [15, 7, 13, 5]])

# ---------------- Sky gradient (period-4 dither -> repeating tiles) --------
stops = [(0, 1), (16, 3), (32, 2), (46, 4), (60, 5), (74, 6),
         (88, 7), (100, 15), (112, 8), (128, 8)]

def sky_color(x, y):
    for i in range(len(stops) - 1):
        y0, c0 = stops[i]
        y1, c1 = stops[i + 1]
        if y0 <= y < y1:
            t = (y - y0) / (y1 - y0)
            return c1 if BAYER[y % 4][x % 4] < t * 16 else c0
    return stops[-1][1]

for y in range(128):
    for x in range(W):
        img[y, x] = sky_color(x, y)

# ---------------- Clouds: thin streaks lit from below ----------------------
clouds = [(52, 30, 46, 4), (196, 22, 40, 3), (90, 56, 34, 3), (220, 62, 30, 3)]
for cx, cy, rx, ry in clouds:
    for y in range(cy - ry, cy + ry + 1):
        for x in range(cx - rx, cx + rx + 1):
            if 0 <= x < W:
                d = ((x - cx) / rx) ** 2 + ((y - cy) / ry) ** 2
                if d <= 1.0:
                    if y >= cy + ry - 1:
                        img[y, x] = 15 if cy > 40 else 5   # lit underside
                    elif y >= cy:
                        img[y, x] = 5 if cy > 40 else 4
                    else:
                        img[y, x] = 4 if cy > 40 else 3

# ---------------- Sun: striped retro disc --------------------------------
SX, SY, SR = 128, 100, 22
for y in range(SY - SR, SY + SR + 1):
    for x in range(SX - SR, SX + SR + 1):
        d = math.hypot(x - SX, y - SY)
        if d <= SR:
            dy = y - SY
            if dy > 2:
                gap = 1 + dy // 7          # stripes widen toward the bottom
                if (dy % 6) < gap:
                    continue
            img[y, x] = 9 if d < SR - 4 else 8

# ---------------- Far mountains ------------------------------------------
def hf(x):
    return (94 - 16 * math.sin(x * 0.021 + 0.6)
            - 8 * math.sin(x * 0.057 + 1.9)
            - 4 * math.sin(x * 0.13 + 0.3))

for x in range(W):
    top = int(round(hf(x)))
    slope = hf(x + 1) - hf(x - 1)
    facing_sun = (x < SX and slope > 0) or (x >= SX and slope < 0)
    for y in range(max(top, 0), 128):
        depth = y - top
        if facing_sun:
            c = 13 if depth < 2 else 10
        else:
            c = 11 if depth < 6 or BAYER[y % 4][x % 4] < 8 else 10
        img[y, x] = c

# ---------------- Near mountains -----------------------------------------
def hn(x):
    return (112 - 9 * math.sin(x * 0.033 + 2.4)
            - 6 * math.sin(x * 0.081 + 0.7)
            - 2 * math.sin(x * 0.21))

for x in range(W):
    top = int(round(hn(x)))
    slope = hn(x + 1) - hn(x - 1)
    facing_sun = (x < SX and slope > 0) or (x >= SX and slope < 0)
    for y in range(max(top, 0), 128):
        depth = y - top
        if depth == 0 and facing_sun:
            c = 4
        elif y > 118:
            t = (y - 118) / 10
            c = 1 if BAYER[y % 4][x % 4] < t * 16 else 2
        else:
            c = 2
        img[y, x] = c

# ---------------- Shoreline with conifers (period 16 -> tile reuse) -------
tree_h = [5, 8, 6, 9, 4, 7, 8, 5]
for x in range(W):
    for y in range(124, 128):
        img[y, x] = 0
    seg = (x // 16) % 8
    lx = x % 16
    h = tree_h[seg]
    half = 3
    dx = abs(lx - 8)
    if dx <= half:
        th = h - dx * 2
        for y in range(124 - th, 124):
            if y >= 0:
                img[y, x] = 0 if dx > 0 or y > 124 - th else 12

# ---------------- Lake: rippled reflection -------------------------------
LAKE_MAP = {0: 0, 1: 1, 2: 1, 3: 1, 4: 2, 5: 4, 6: 5, 7: 6, 8: 7, 9: 8,
            10: 3, 11: 1, 12: 0, 13: 4, 14: 14, 15: 6}
for y in range(128, H):
    d = y - 128
    off = int(round(1.5 * math.sin(d * 0.9)))      # whole-row ripple shift
    src_y = max(0, 127 - d)
    for x in range(W):
        sx = min(W - 1, max(0, x + off))
        c = LAKE_MAP[int(img[src_y, sx])]
        # blue ripple lines across the water
        if d > 6 and (d % 5) == 0 and (x + d * 3) % 8 < 5:
            c = 14 if c not in (8, 7) else 9
        img[y, x] = c

# sun glitter column
for y in range(130, H):
    d = y - 128
    width = 10 + d // 3
    if d % 3 == 0:
        for x in range(SX - width, SX + width):
            if (x // 4 + d) % 3 != 0:
                img[y, x] = 8 if abs(x - SX) < width // 2 else 15

# ---------------- Tile budget enforcement --------------------------------
def tiles_of(a):
    out = []
    for ty in range(H // 8):
        for tx in range(W // 8):
            out.append(a[ty*8:ty*8+8, tx*8:tx*8+8].copy())
    return out

def canon(t):
    vs = [t, t[:, ::-1], t[::-1, :], t[::-1, ::-1]]
    return min(v.tobytes() for v in vs)

tiles = tiles_of(img)
keys = [canon(t) for t in tiles]
counts = {}
rep = {}
for k, t in zip(keys, tiles):
    counts[k] = counts.get(k, 0) + 1
    rep.setdefault(k, t)

if len(counts) > TILE_LIMIT:
    pal_np = np.array(PAL, dtype=np.int32)
    cdist = ((pal_np[:, None, :] - pal_np[None, :, :]) ** 2).sum(-1)
    order = sorted(counts, key=lambda k: -counts[k])
    keep = order[:TILE_LIMIT]
    keep_variants = []
    for k in keep:
        t = rep[k]
        for v in (t, t[:, ::-1], t[::-1, :], t[::-1, ::-1]):
            keep_variants.append(v)
    kv = np.array(keep_variants)
    keep_set = set(keep)
    for i, (k, t) in enumerate(zip(keys, tiles)):
        if k not in keep_set:
            err = cdist[kv, t[None]].sum(axis=(1, 2))
            best = kv[int(np.argmin(err))]
            ty, tx = divmod(i, W // 8)
            img[ty*8:ty*8+8, tx*8:tx*8+8] = best

final_unique = len({canon(t) for t in tiles_of(img)})
assert final_unique <= TILE_LIMIT, final_unique
assert len(np.unique(img)) <= 16

out = Image.fromarray(img, mode="P")
flat = [v for c in PAL for v in c]
out.putpalette(flat + [0] * (768 - len(flat)))
out.save("art.png")
print(f"Saved art.png: {W}x{H}, {len(np.unique(img))} colors, "
      f"{final_unique} unique tiles (flips merged)")
