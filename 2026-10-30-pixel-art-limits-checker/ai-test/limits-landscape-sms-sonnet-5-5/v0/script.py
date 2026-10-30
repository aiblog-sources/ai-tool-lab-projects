import numpy as np
from PIL import Image

W, H = 256, 192
HOR = 120

# SMS colours: 2 bits per channel (0, 85, 170, 255). 11 colours in total, all in one 16-colour palette.
S0 = (0, 0, 85); S1 = (85, 0, 85); S2 = (170, 0, 85); S3 = (255, 85, 85)
S4 = (255, 85, 0); S5 = (255, 170, 0); S6 = (255, 255, 85); SUN = (255, 255, 170)
F = (170, 0, 170); M = (85, 0, 85); N = (0, 0, 0); WHITE = (255, 255, 255)

img = np.zeros((H, W, 3), np.uint8)
bayer = np.array([[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]]) / 16.0

def grad(cols, p, x, y):
    p = min(max(p, 0), 0.9999) * (len(cols) - 1)
    i = int(p)
    return cols[i + 1] if (p - i) > bayer[y % 4][x % 4] else cols[i]

sky = [S0, S1, S2, S3, S4, S5, S6]
for y in range(HOR):
    for x in range(W):
        img[y, x] = grad(sky, y / HOR, x, y)

# a few stars in the dark top part
for (sx, sy) in [(12, 6), (40, 20), (75, 9), (100, 30), (160, 12), (190, 26), (220, 8), (240, 34), (30, 44), (205, 48)]:
    img[sy, sx] = WHITE

# sun
cx, cy, r = 128, 108, 22
for y in range(HOR):
    for x in range(W):
        if (x - cx) ** 2 + (y - cy) ** 2 <= r * r:
            img[y, x] = SUN if (x - cx) ** 2 + (y - cy) ** 2 <= 14 * 14 else S6

def ridge(x, base, a1, f1, a2, f2, ph):
    return int(round(base + a1 * np.sin(x / f1 + ph) + a2 * np.sin(x / f2 + ph * 2)))

def tops(base, a1, f1, a2, f2, ph):
    return [HOR - ridge(x, base, a1, f1, a2, f2, ph) for x in range(W)]

far = tops(28, 14, 31, 7, 11, 0.5)
mid = tops(16, 9, 23, 4, 7, 2.0)
near = [HOR - int(round(3 + 14 * max(0, np.cos(x / 40.0 + 0.2)) ** 3 + 12 * max(0, np.cos((x - 256) / 35.0)) ** 3)) for x in range(W)]

for x in range(W):
    for y in range(far[x], HOR):
        img[y, x] = F
    for y in range(mid[x], HOR):
        img[y, x] = M
    for y in range(near[x], HOR):
        img[y, x] = N

# lake: mirrored scenery on a gradient
lake = [S3, S2, S1, S0]
for y in range(HOR, H):
    d = y - HOR
    for x in range(W):
        img[y, x] = grad(lake, d / (H - HOR), x, y)
# sun reflection streaks
for y in range(HOR + 1, H):
    d = y - HOR
    w = 22 - d // 5
    if d % 3 != 2 and w > 0:
        for x in range(128 - w, 128 + w):
            if ((x // 4) + d) % 3 != 0:
                img[y, x] = S5 if d < 30 else S4
# mirrored mountains
for x in range(W):
    for layer, col in ((far, F), (mid, M), (near, N)):
        h = HOR - layer[x]
        for d in range(min(h, H - HOR)):
            y = HOR + 1 + d
            if y < H and d % 4 != 3:
                img[y, x] = col
# ripple lines (periodic in x)
for y in range(HOR + 4, H, 6):
    for x in range(W):
        if (x % 16) < 6:
            img[y, x] = S0 if y > HOR + 30 else S1

# --- hardware check: unique 8x8 tiles (with flips) must be <= 448 ---
seen = set()
for ty in range(H // 8):
    for tx in range(W // 8):
        t = img[ty * 8:ty * 8 + 8, tx * 8:tx * 8 + 8]
        variants = [t, t[:, ::-1], t[::-1], t[::-1, ::-1]]
        keys = [v.tobytes() for v in variants]
        if not any(k in seen for k in keys):
            seen.add(keys[0])
ncol = len({tuple(c) for c in img.reshape(-1, 3)})
print("unique tiles (with flips):", len(seen), "colours:", ncol)
assert len(seen) <= 448 and ncol <= 16

Image.fromarray(img, "RGB").save("art.png")
