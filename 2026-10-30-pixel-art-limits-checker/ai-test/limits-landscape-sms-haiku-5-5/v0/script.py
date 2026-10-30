# Sega Master System sunset background, 256x192, saved as art.png.
#
# SMS constraints respected:
#   - Colors: 6-bit RGB, 2 bits per channel (levels 0..3 -> 0, 85, 170, 255).
#   - Palette: one 16-entry background palette (palette 0). Every 8x8 tile
#     uses only that palette, so no tile exceeds 16 colors.
#   - Tiles: the 32x24 tile grid is deduplicated and checked against a
#     256-tile budget for background pattern data.
#   - Mountain edges are aligned to 8-pixel columns and sky/lake bands are
#     horizontal, which keeps the unique tile count low.

import numpy as np
from PIL import Image

W, H = 256, 192
HORIZON = 96
TILE = 8
MAX_TILES = 256

# 16-entry background palette, each entry as (R, G, B) with levels 0..3.
PAL = [
    (0, 0, 1),  # 0  deep night blue (top of sky)
    (0, 0, 2),  # 1  blue-violet
    (1, 0, 2),  # 2  purple
    (2, 0, 2),  # 3  magenta
    (3, 0, 2),  # 4  pink-red
    (3, 1, 1),  # 5  red-orange
    (3, 2, 1),  # 6  orange
    (3, 3, 2),  # 7  pale yellow (sun ring, reflection)
    (3, 3, 3),  # 8  white (sun core)
    (1, 0, 1),  # 9  front mountain
    (0, 0, 0),  # 10 black (spare)
    (2, 1, 2),  # 11 back mountain (mauve)
    (1, 1, 2),  # 12 lake blue
    (0, 1, 2),  # 13 deep lake blue
    (2, 1, 0),  # 14 dark gold lake band
    (2, 1, 1),  # 15 muted spare
]
assert len(PAL) <= 16
assert all(0 <= v <= 3 for c in PAL for v in c)

idx = np.zeros((H, W), dtype=np.uint8)
xs = np.arange(W)
yy, xx = np.mgrid[0:H, 0:W]

# --- Sky: horizontal bands from deep blue at top to orange near the horizon.
sky_seq = [0, 1, 2, 3, 4, 5, 6, 7]
for y in range(HORIZON):
    k = min(y * len(sky_seq) // HORIZON, len(sky_seq) - 1)
    idx[y, :] = sky_seq[k]

# --- Sun: disc above the horizon with a bright core.
cx, cy, r = 160, 88, 14
d2 = (xx - cx) ** 2 + (yy - cy) ** 2
sun_mask = (d2 <= r * r) & (yy < HORIZON)
idx[sun_mask] = 7
core_mask = (d2 <= (r // 2) ** 2) & (yy < HORIZON)
idx[core_mask] = 8

# --- Mountains: two ridges, height chosen per 8-pixel column so every
# tile is uniform across its width.
rng = np.random.default_rng(7)
cols = np.arange(W // TILE)
back_h = np.round(58 + 10 * np.sin(cols * 0.45) + 6 * np.sin(cols * 1.1 + 1)
                  + rng.integers(-3, 4, cols.size)).astype(int)
back_h = np.clip(back_h, 40, 90)
front_h = np.round(72 + 6 * np.sin(cols * 0.6 + 2)
                   + rng.integers(-2, 3, cols.size)).astype(int)
front_h = np.clip(front_h, 66, 92)

for c in cols:
    x0, x1 = c * TILE, c * TILE + TILE
    idx[back_h[c]:HORIZON, x0:x1] = 11
for c in cols:
    x0, x1 = c * TILE, c * TILE + TILE
    idx[front_h[c]:HORIZON, x0:x1] = 9

# --- Lake: horizontal color bands below the horizon.
lake_seq = [6, 14, 12, 13, 12, 13]
for y in range(HORIZON, H):
    k = min((y - HORIZON) // 16, len(lake_seq) - 1)
    idx[y, :] = lake_seq[k]

# --- Sun reflection on the water: a tapering column with ripple gaps.
for y in range(HORIZON, H):
    w = 14 - (y - HORIZON) * 0.5
    if w <= 0:
        break
    dx = np.abs(xs - cx)
    row = idx[y]
    if y % 2:
        m = (dx <= w) & (xs % 4 != 0)
    else:
        m = dx <= w
    row[m] = 7
    core = (dx <= w / 2) & (y % 2 == 0)
    row[core] = 8

# --- Convert to RGB using the SMS 6-bit levels.
rgb_lut = np.array([[v * 85 for v in c] for c in PAL], dtype=np.uint8)
out = rgb_lut[idx]

# --- Verify tile budget: each 8x8 tile must be unique within the set.
tiles = set()
for ty in range(H // TILE):
    for tx in range(W // TILE):
        tiles.add(idx[ty * TILE:(ty + 1) * TILE, tx * TILE:(tx + 1) * TILE].tobytes())
assert len(tiles) <= MAX_TILES, f"too many unique tiles: {len(tiles)}"
print(f"unique background tiles: {len(tiles)} / {MAX_TILES}")

Image.fromarray(out, "RGB").save("art.png")
