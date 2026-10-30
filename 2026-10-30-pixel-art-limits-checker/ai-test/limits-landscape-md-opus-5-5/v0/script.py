#!/usr/bin/env python3
# Sunset over mountains and a lake, Sega Mega Drive / Genesis, 320x224.
# Hardware rules followed:
# - Every colour comes from the 9-bit MD space (3 bits per channel).
# - 3 palettes of 15 colours. Index 0 is left as the backdrop/transparent slot.
# - Each 8x8 tile uses colours from one palette only (checked at the end).
# - Unique tile count (flips counted as duplicates) is printed. It fits in
#   VRAM with room left for maps and sprites.
import numpy as np
from PIL import Image

W, H = 320, 224
HZ = 140                      # horizon / lake surface
rng = np.random.default_rng(7)

def lv(v):                    # 3-bit level -> 8-bit value
    return round(v * 255 / 7)

def C(r, g, b):
    return (lv(r), lv(g), lv(b))

# ---------------- palettes (15 usable colours each) ----------------
SKY = [C(1,0,2), C(2,0,3), C(3,1,4), C(4,1,4), C(5,1,4), C(6,2,3),
       C(7,3,2), C(7,4,1), C(7,5,1), C(7,6,2), C(7,7,4)]
SUN = C(7,7,6)
CL_DARK, CL_PINK, CL_LIT = C(2,1,3), C(6,3,4), C(7,5,3)
P0 = SKY + [SUN, CL_DARK, CL_PINK, CL_LIT]

FAR_LIT, FAR_MID, FAR_DK = C(5,2,4), C(4,2,4), C(3,1,4)
NEAR_LIT, NEAR, NEAR_DK = C(2,1,3), C(1,0,2), C(0,0,1)
RIM = C(7,4,2)
P1 = [FAR_LIT, FAR_MID, FAR_DK, NEAR_LIT, NEAR, NEAR_DK, RIM] + SKY[4:] + [SUN]

P2 = [C(0,0,1), C(1,0,2), C(2,0,3), C(3,1,4), C(4,1,4), C(5,1,4),
      C(6,2,3), C(7,3,2), C(7,4,1), C(7,5,1), C(7,6,2), C(7,7,4),
      SUN, C(2,1,3), C(1,1,2)]

PALS = [P0, P1, P2]
for p in PALS:
    assert len(p) <= 15 and len(set(p)) == len(p)

img = np.zeros((H, W, 3), np.uint8)

# ---------------- sky ----------------
bayer = np.array([[0, 8, 2, 10], [12, 4, 14, 6],
                  [3, 11, 1, 9], [15, 7, 13, 5]]) / 16.0
SCX, SCY, SR = 160, 116, 22
for y in range(HZ):
    for x in range(W):
        d = np.hypot((x - SCX) * 0.8, (y - SCY) * 1.3)
        t = y / HZ * 10 + max(0.0, 1 - d / 95) * 2.6
        i = int(np.clip(np.floor(t + bayer[y % 4, x % 4]), 0, 10))
        img[y, x] = SKY[i]

# ---------------- sun (with retro cut-out bands) ----------------
gaps = {6, 10, 13, 16, 18, 20}
for y in range(SCY - SR, SCY + SR + 1):
    for x in range(SCX - SR, SCX + SR + 1):
        d = np.hypot(x - SCX, y - SCY)
        if d <= SR and (y - SCY) not in gaps:
            img[y, x] = SUN if d < SR - 3 else SKY[10]

# ---------------- clouds (kept above the mountain tile rows) ----------------
clouds = [(60, 30, 46, 4), (95, 36, 30, 3), (250, 24, 50, 4),
          (215, 32, 26, 3), (150, 50, 60, 3), (290, 52, 30, 2),
          (30, 54, 34, 2)]
mask = np.zeros((H, W), bool)
yy, xx = np.mgrid[0:H, 0:W]
for cx, cy, rx, ry in clouds:
    lump = 1 + 0.35 * np.sin(xx * 0.21 + cx) * (yy < cy)
    mask |= ((xx - cx) / rx) ** 2 + ((yy - cy) / (ry * lump)) ** 2 < 1
mask[60:] = False
below = np.zeros_like(mask); below[:-2] = mask[2:]
for y, x in zip(*np.nonzero(mask)):
    near_sun = abs(x - SCX) < 110
    if not below[y, x]:
        img[y, x] = CL_LIT if near_sun else CL_PINK
    elif not mask[y + 1, x]:
        img[y, x] = CL_PINK
    else:
        img[y, x] = CL_DARK

# ---------------- mountain ridges ----------------
def midpoint(amp, rough, seed):
    r = np.random.default_rng(seed)
    n = 512
    a = np.zeros(n + 1)
    a[0], a[n] = r.uniform(-1, 1, 2) * amp
    step, s = n, amp
    while step > 1:
        h = step // 2
        for i in range(h, n, step):
            a[i] = (a[i - h] + a[i + h]) / 2 + r.uniform(-1, 1) * s
        s *= rough
        step = h
    return a[:W + 1]

xs = np.arange(W + 1)
far = 98 + 12 * np.sin(xs / 48 + 0.6) + midpoint(16, 0.55, 3)
far = np.clip(far, 66, HZ - 6).astype(int)
near = HZ - 5 - 48 * (np.abs(xs - 160) / 160) ** 1.4 + midpoint(9, 0.5, 11)
near = np.clip(near, 82, HZ - 2).astype(int)

def faces_sun(prof, x):
    s = prof[x + 1] - prof[x]
    return (x < SCX and s > 0) or (x >= SCX and s < 0)

for x in range(W):
    lit = faces_sun(far, x)
    for y in range(far[x], HZ):
        c = FAR_LIT if lit else FAR_MID
        if not lit and (y - far[x]) > 14 and bayer[y % 4, x % 4] < 0.5:
            c = FAR_DK
        if HZ - y < 7 and bayer[y % 4, x % 4] < (7 - (HZ - y)) / 7:
            c = FAR_LIT                       # haze near the water
        img[y, x] = c
for x in range(W):
    lit = faces_sun(near, x)
    for y in range(near[x], HZ):
        dy = y - near[x]
        if dy == 0 and lit:
            c = RIM
        elif lit and dy < 6 and bayer[y % 4, x % 4] < (6 - dy) / 6:
            c = NEAR_LIT
        elif dy > 22:
            c = NEAR_DK
        else:
            c = NEAR
        img[y, x] = c

# ---------------- lake reflection ----------------
P2a = np.array(P2, float)
def nearest(col, pal_arr):
    return tuple(int(v) for v in pal_arr[np.argmin(((pal_arr - col) ** 2).sum(1))])

dark_cache = {}
for y in range(HZ, H):
    depth = y - HZ
    sy = HZ - 1 - int(depth * 1.05)
    sy = max(sy, 0)
    off = int(round((1 + depth / 30) * np.sin(y * 1.7)))
    fade = 0.78 - depth / H * 0.35
    for x in range(W):
        src = tuple(img[sy, (x + off) % W])
        key = (src, round(fade, 2))
        if key not in dark_cache:
            dark_cache[key] = nearest(np.array(src, float) * fade + [0, 0, 18], P2a)
        img[y, x] = dark_cache[key]
    if depth % 4 == 1:                       # thin highlight ripple line
        for x in range(0, W, 1):
            if (x // 6 + y) % 5 == 0:
                c = np.array(img[y, x], float)
                img[y, x] = nearest(c * 1.25, P2a)

# sun glitter column
for y in range(HZ, H, 2):
    depth = y - HZ
    w = SR + 2 - depth * 0.14
    x = int(SCX - w)
    while x < SCX + w:
        seg = int(rng.integers(3, 11))
        col = SUN if depth < 25 else (P2[11] if depth < 55 else P2[10])
        if rng.random() < 0.7:
            img[y, max(0, x):min(W, x + seg)] = col
        x += seg + int(rng.integers(2, 6))

# ---------------- foreground shore with pines ----------------
SHORE = P2[0]
for x in range(W):
    yl = 186 + 0.0025 * x * x if x < 130 else H
    yr = 214 - 0.004 * (W - x) ** 2 if x > 260 else H
    top = int(min(yl, yr))
    if top < H:
        img[top:, x] = SHORE
for tx, base, ht in [(14, 190, 40), (34, 192, 30), (52, 195, 24), (6, 188, 30),
                     (76, 202, 16), (306, 210, 26), (292, 214, 14)]:
    for y in range(base - ht, base):
        t = (y - (base - ht)) / ht
        hw = int(1 + t * ht * 0.28 * (0.75 + 0.25 * ((y // 3) % 2)))
        img[y, max(0, tx - hw):min(W, tx + hw + 1)] = SHORE
    img[base - 2:base + 3, tx] = SHORE

# ---------------- per-tile palette fit + checks ----------------
pal_arrs = [np.array(p, float) for p in PALS]
out = img.copy()
tiles = set()
for ty in range(H // 8):
    for tx in range(W // 8):
        blk = img[ty*8:ty*8+8, tx*8:tx*8+8].reshape(-1, 3).astype(float)
        best = None
        for pi, pa in enumerate(pal_arrs):
            d = ((blk[:, None, :] - pa[None]) ** 2).sum(2)
            err = d.min(1).sum()
            if best is None or err < best[0]:
                best = (err, pi, d.argmin(1))
        _, pi, idx = best
        out[ty*8:ty*8+8, tx*8:tx*8+8] = np.array(PALS[pi], np.uint8)[idx].reshape(8, 8, 3)
        t = (idx + 1).reshape(8, 8)
        variants = [t, t[:, ::-1], t[::-1], t[::-1, ::-1]]
        tiles.add(min(v.tobytes() for v in variants))

colors = {tuple(c) for c in out.reshape(-1, 3)}
assert len(colors) <= 45
assert all(v in [lv(i) for i in range(8)] for c in colors for v in c)
print(f"colours used: {len(colors)}, palettes: {len(PALS)}, unique tiles: {len(tiles)}")

Image.fromarray(out, "RGB").save("art.png")
