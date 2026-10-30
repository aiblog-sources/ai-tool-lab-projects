#!/usr/bin/env python3
"""Sunset over mountains and a lake, SNES full-screen background (256x224).
Colors are snapped to the SNES 15-bit palette, and every 8x8 tile is
restricted to one of 8 palettes of at most 15 colors."""

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

# ------------------------------------------------------------------ SNES palette fitting
NPAL, NCOL, T = 8, 15, 8

def snap(c):
    return np.clip(np.round(np.asarray(c, dtype=np.float32) / 8.0) * 8, 0, 248)

src = (np.clip(img, 0, 255).astype(np.uint8) & 0xF8).astype(np.float32)
TY, TX = H // T, W // T
tiles = src.reshape(TY, T, TX, T, 3).transpose(0, 2, 1, 3, 4).reshape(TY * TX, T * T, 3)
NT = tiles.shape[0]

def build_palette(pixels):
    """Weighted k-means over unique colors -> at most NCOL snapped colors."""
    cols, counts = np.unique(pixels.reshape(-1, 3), axis=0, return_counts=True)
    cols = cols.astype(np.float32)
    w = counts.astype(np.float32)
    if len(cols) <= NCOL:
        return cols
    # farthest-point init starting at most frequent color (keeps highlights)
    centers = [cols[np.argmax(w)]]
    dmin = ((cols - centers[0]) ** 2).sum(1)
    while len(centers) < NCOL:
        i = int(np.argmax(dmin))
        centers.append(cols[i])
        dmin = np.minimum(dmin, ((cols - cols[i]) ** 2).sum(1))
    centers = np.array(centers)
    for _ in range(15):
        d = ((cols[:, None, :] - centers[None]) ** 2).sum(2)
        lab = d.argmin(1)
        for k in range(NCOL):
            m = lab == k
            if m.any():
                centers[k] = (cols[m] * w[m, None]).sum(0) / w[m].sum()
    centers = snap(centers)
    return np.unique(centers, axis=0)

def tile_errors(pal):
    d = ((tiles[:, :, None, :] - pal[None, None]) ** 2).sum(3)
    return d.min(2).sum(1)

# initial grouping: k-means on tile mean colors (farthest-point init)
feat = tiles.mean(1)
cent = [feat[0]]
dm = ((feat - cent[0]) ** 2).sum(1)
while len(cent) < NPAL:
    i = int(np.argmax(dm))
    cent.append(feat[i])
    dm = np.minimum(dm, ((feat - feat[i]) ** 2).sum(1))
cent = np.array(cent)
for _ in range(10):
    assign = ((feat[:, None] - cent[None]) ** 2).sum(2).argmin(1)
    for k in range(NPAL):
        if (assign == k).any():
            cent[k] = feat[assign == k].mean(0)

palettes = None
for it in range(8):
    palettes = []
    for k in range(NPAL):
        members = assign == k
        if not members.any():
            # reseed an empty palette from the worst-fitting tile
            if palettes:
                errs = np.min([tile_errors(p) for p in palettes], axis=0)
                members = np.zeros(NT, bool)
                members[int(np.argmax(errs))] = True
            else:
                members[0] = True
        palettes.append(build_palette(tiles[members]))
    errs = np.stack([tile_errors(p) for p in palettes], 1)
    new_assign = errs.argmin(1)
    if np.array_equal(new_assign, assign) and it > 0:
        break
    assign = new_assign

# final remap
outt = np.empty_like(tiles)
for t in range(NT):
    pal = palettes[assign[t]]
    d = ((tiles[t][:, None, :] - pal[None]) ** 2).sum(2)
    outt[t] = pal[d.argmin(1)]

out = outt.reshape(TY, TX, T, T, 3).transpose(0, 2, 1, 3, 4).reshape(H, W, 3)
out = (np.clip(out, 0, 248).astype(np.uint8) & 0xF8)
Image.fromarray(out, "RGB").save("art.png")
