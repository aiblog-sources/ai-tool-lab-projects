#!/usr/bin/env python3
"""Sunset over mountains and a lake, SNES full-screen background (256x224).

Hardware-legal for a 4bpp BG layer (e.g. BG1 in mode 1):
  * 32x28 tiles of 8x8 pixels (896 tiles, 28 KB of VRAM)
  * every tile uses exactly one of 8 sub-palettes of 16 colors
  * color index 0 is shared by all sub-palettes
  * all colors are SNES 15-bit BGR (5 bits per channel)
"""

import math
import numpy as np
from PIL import Image

W, H = 256, 224
HORIZON = 150  # first row of the lake

img = np.zeros((H, W, 3), dtype=np.float32)

BAYER4 = (np.array([[0, 8, 2, 10],
                    [12, 4, 14, 6],
                    [3, 11, 1, 9],
                    [15, 7, 13, 5]], dtype=np.float32) + 0.5) / 16.0

# ------------------------------------------------------------------ sky
SKY = [(24, 16, 56), (48, 24, 88), (88, 32, 112), (144, 48, 120),
       (200, 72, 104), (232, 112, 80), (248, 160, 72), (248, 200, 104)]
n = len(SKY)
for y in range(HORIZON):
    t = (y / (HORIZON - 1)) ** 0.85 * (n - 1)
    i = min(int(t), n - 2)
    f = t - i
    for x in range(W):
        c = SKY[i + 1] if f > BAYER4[y % 4, x % 4] else SKY[i]
        img[y, x] = c

# ------------------------------------------------------------------ sun
SUN_CX, SUN_CY, SUN_R = 128, 128, 26
for y in range(SUN_CY - SUN_R - 3, SUN_CY + SUN_R + 1):
    for x in range(SUN_CX - SUN_R - 3, SUN_CX + SUN_R + 4):
        d = math.hypot(x - SUN_CX, y - SUN_CY)
        if d <= SUN_R:
            dy = y - SUN_CY
            if dy > 4:
                k = dy - 4
                period = max(3, 8 - k // 4)
                gap = 1 + k // 8
                if k % period < gap:
                    continue
            img[y, x] = (248, 240, 176) if d < SUN_R - 3 else (248, 208, 120)
        elif d <= SUN_R + 3 and 0 <= y < HORIZON:
            if BAYER4[y % 4, x % 4] < 0.5:
                img[y, x] = (248, 184, 96)

# ------------------------------------------------------------------ clouds
def cloud(cx, cy, length, rows=4):
    for r in range(rows):
        half = int(length * (1 - (abs(r - rows / 2 + 0.5) / rows) ** 1.5) / 2)
        y = cy + r
        off = (r * 5) % 7 - 3
        for x in range(cx - half + off, cx + half + off):
            if 0 <= x < W and 0 <= y < HORIZON:
                if r == 0:
                    col = (248, 184, 160)
                elif r == rows - 1:
                    col = (136, 48, 112)
                else:
                    col = (224, 112, 136)
                img[y, x] = col

cloud(40, 30, 50, 3)
cloud(190, 22, 70, 4)
cloud(110, 52, 40, 3)
cloud(220, 68, 46, 3)
cloud(25, 84, 60, 3)
cloud(160, 96, 34, 2)

# ------------------------------------------------------------------ mountains
def ridge(base, amp, rough, seed):
    rng = np.random.default_rng(seed)
    size = 257
    h = np.zeros(size)
    h[0] = rng.uniform(-1, 1)
    h[-1] = rng.uniform(-1, 1)
    step, scale = size - 1, 1.0
    while step > 1:
        half = step // 2
        for i in range(half, size - 1, step):
            h[i] = (h[i - half] + h[i + half]) / 2 + rng.uniform(-scale, scale)
        step = half
        scale *= rough
    h = (h - h.min()) / (h.max() - h.min() + 1e-9)
    return np.clip(base - h * amp, 0, HORIZON - 1).astype(int)[:W]

def draw_range(heights, body, rim, rim2=None):
    for x in range(W):
        top = heights[x]
        img[top:HORIZON, x] = body
        img[top, x] = rim
        if rim2 is not None and x + 1 < W:
            facing = (x < SUN_CX and heights[x + 1] > top) or \
                     (x > SUN_CX and x > 0 and heights[x - 1] > top)
            if facing and top + 1 < HORIZON:
                img[top + 1, x] = rim2

far = ridge(112, 46, 0.58, 7)
draw_range(far, (112, 56, 112), (200, 112, 120), (160, 80, 120))

mid = ridge(132, 30, 0.55, 21)
draw_range(mid, (64, 32, 80), (128, 56, 96), (96, 40, 88))

near = ridge(146, 12, 0.6, 99)
rng = np.random.default_rng(5)
trees = np.array(near, dtype=int)
x = 0
while x < W:
    th = rng.integers(5, 13)
    tw = th // 2
    for dx in range(-tw, tw + 1):
        xx = x + dx
        if 0 <= xx < W:
            peak = near[min(max(x, 0), W - 1)] - th + abs(dx) * 2
            trees[xx] = min(trees[xx], max(peak, 0))
    x += rng.integers(4, 11)
draw_range(trees, (32, 16, 48), (56, 24, 64))

# ------------------------------------------------------------------ lake
scene = img.copy()
WATER = np.array((24, 32, 80), dtype=np.float32)
depth = H - HORIZON
for y in range(HORIZON, H):
    d = y - HORIZON
    sy = max(HORIZON - 1 - int(d * 1.6), 0)
    wobble = int(round(math.sin(y * 0.9) * (1 + d / 25.0)))
    row = np.roll(scene[sy], wobble, axis=0)
    k = 0.35 + 0.35 * d / depth
    img[y] = row * (1 - k) + WATER * k

rng = np.random.default_rng(3)
for y in range(HORIZON + 1, H):
    d = y - HORIZON
    if d % 2:
        continue
    half = int(6 + d * 0.45)
    for x in range(SUN_CX - half, SUN_CX + half + 1):
        edge = abs(x - SUN_CX) / half
        if rng.random() < 0.55 * (1 - edge):
            img[y, x] = (248, 224, 152) if edge < 0.4 else (232, 152, 96)

for y in range(HORIZON + 3, H, 5):
    d = y - HORIZON
    seglen = 3 + d // 10
    x = int(rng.integers(0, 12))
    while x < W:
        img[y, x:x + seglen] = np.minimum(img[y, x:x + seglen] + 40, 248)
        x += seglen + int(rng.integers(10, 30))

img[HORIZON] = (16, 8, 32)

# ================================================================== SNES conversion
TILE = 8
NPAL = 8
PALSIZE = 16
SHARED = np.array([16, 8, 32], dtype=np.float64)   # color index 0, common to all palettes
WEIGHT = np.array([3.0, 4.0, 2.0])                 # rough perceptual channel weights


def snap15(c):
    """Round to the nearest SNES 15-bit color (8-bit value multiple of 8, max 248)."""
    return np.clip(np.round(np.asarray(c, dtype=np.float64) / 8.0) * 8.0, 0, 248)


def build_palette(pix, k=PALSIZE - 1):
    """Weighted k-means on the unique colors -> up to k snapped 15-bit colors."""
    uniq, counts = np.unique(pix, axis=0, return_counts=True)
    uniq = uniq.astype(np.float64)
    counts = counts.astype(np.float64)
    # the shared color is already available as index 0
    keep = np.any(uniq != SHARED, axis=1)
    uniq, counts = uniq[keep], counts[keep]
    if len(uniq) <= k:
        return uniq
    # deterministic farthest-point initialisation
    centers = [uniq[np.argmax(counts)]]
    mind = (((uniq - centers[0]) ** 2) * WEIGHT).sum(1)
    for _ in range(k - 1):
        i = int(np.argmax(mind * np.sqrt(counts)))
        centers.append(uniq[i])
        mind = np.minimum(mind, (((uniq - uniq[i]) ** 2) * WEIGHT).sum(1))
    centers = np.array(centers)
    for _ in range(15):
        d = (((uniq[:, None, :] - centers[None]) ** 2) * WEIGHT).sum(-1)
        lab = d.argmin(1)
        for j in range(len(centers)):
            m = lab == j
            if m.any():
                centers[j] = (uniq[m] * counts[m, None]).sum(0) / counts[m].sum()
    centers = np.unique(snap15(centers), axis=0)
    return centers[:k]


def full_palette(colors):
    pal = np.vstack([SHARED[None], colors]) if len(colors) else SHARED[None].copy()
    while len(pal) < PALSIZE:          # pad unused entries with the shared color
        pal = np.vstack([pal, SHARED[None]])
    return pal


def tile_errors(tiles, pal):
    d = (((tiles[:, :, None, :] - pal[None, None]) ** 2) * WEIGHT).sum(-1)
    return d.min(-1).sum(-1)


rgb = snap15(img)
TY, TX = H // TILE, W // TILE
tiles = rgb.reshape(TY, TILE, TX, TILE, 3).transpose(0, 2, 1, 3, 4).reshape(-1, TILE * TILE, 3)
NT = len(tiles)

# initial grouping: k-means on tile mean colors
means = tiles.mean(1)
lum = (means * WEIGHT).sum(1)
cent = [means[np.argmin(lum)]]
md = (((means - cent[0]) ** 2) * WEIGHT).sum(1)
for _ in range(NPAL - 1):
    i = int(np.argmax(md))
    cent.append(means[i])
    md = np.minimum(md, (((means - means[i]) ** 2) * WEIGHT).sum(1))
cent = np.array(cent)
for _ in range(10):
    assign = ((((means[:, None] - cent[None]) ** 2) * WEIGHT).sum(-1)).argmin(1)
    for p in range(NPAL):
        if (assign == p).any():
            cent[p] = means[assign == p].mean(0)

# alternate: build palettes for groups, then reassign tiles to their best palette
palettes = None
for it in range(8):
    pals = []
    errs_prev = None
    for p in range(NPAL):
        members = tiles[assign == p]
        if len(members) == 0:
            # reseed an empty group with the worst-represented tile
            if palettes is not None:
                cur = np.array([tile_errors(tiles[t:t + 1], palettes[assign[t]])[0]
                                for t in range(NT)])
                members = tiles[int(np.argmax(cur))][None]
            else:
                members = tiles[p * NT // NPAL][None]
        pals.append(full_palette(build_palette(members.reshape(-1, 3))))
    palettes = np.array(pals)
    errs = np.stack([tile_errors(tiles, palettes[p]) for p in range(NPAL)], 1)
    new_assign = errs.argmin(1)
    if np.array_equal(new_assign, assign) and it > 0:
        break
    assign = new_assign

# final remap: every pixel -> nearest color in its tile's palette
out_tiles = np.empty_like(tiles)
for t in range(NT):
    pal = palettes[assign[t]]
    d = (((tiles[t][:, None, :] - pal[None]) ** 2) * WEIGHT).sum(-1)
    out_tiles[t] = pal[d.argmin(1)]

out = out_tiles.reshape(TY, TX, TILE, TILE, 3).transpose(0, 2, 1, 3, 4).reshape(H, W, 3)
out = out.astype(np.uint8)

Image.fromarray(out, "RGB").save("art.png")
