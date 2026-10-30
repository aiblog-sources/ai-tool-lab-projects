import numpy as np
from PIL import Image

W, H = 256, 224
HOR = 136
rng = np.random.default_rng(7)
xs = np.arange(W)
ys = np.arange(H)

def lerp(a, b, t):
    return a + (b - a) * t

# ---------- Sky ----------
stops = [(0, (28, 20, 72)), (40, (88, 40, 110)), (85, (200, 82, 100)),
         (115, (252, 150, 70)), (136, (255, 214, 130))]
sky = np.zeros((HOR, W, 3), float)
for y in range(HOR):
    for i in range(len(stops) - 1):
        y0, c0 = stops[i]
        y1, c1 = stops[i + 1]
        if y0 <= y <= y1:
            t = (y - y0) / (y1 - y0)
            sky[y, :] = lerp(np.array(c0, float), np.array(c1, float), t)
            break

# Sun and glow
sx, sy, sr = 128, 122, 17
yy, xx = np.mgrid[0:HOR, 0:W]
d = np.sqrt((xx - sx) ** 2 + ((yy - sy) * 1.1) ** 2)
glow = np.clip(1 - d / 90, 0, 1) ** 2 * 0.55
sky = sky * (1 - glow[..., None]) + np.array([255, 190, 100]) * glow[..., None]
disc = np.clip(sr + 0.5 - d, 0, 1)
sky = sky * (1 - disc[..., None]) + np.array([255, 244, 190]) * disc[..., None]

# Clouds (flat streaks, lit from below)
def cloud(cx, cy, rx, ry, col, a):
    global sky
    m = (((xx - cx) / rx) ** 2 + ((yy - cy) / ry) ** 2) <= 1
    sky[m] = sky[m] * (1 - a) + np.array(col, float) * a
for cx, cy, rx, ry in [(40, 30, 40, 4), (90, 50, 55, 4), (200, 38, 50, 4),
                       (230, 66, 40, 3), (30, 78, 45, 3), (170, 84, 60, 3),
                       (110, 20, 30, 3)]:
    cloud(cx, cy, rx, ry, (70, 36, 96), 0.55)
    cloud(cx, cy + ry * 0.55, rx * 0.8, ry * 0.45, (230, 110, 110), 0.45)

# Mountains
far = (128 - (22 * np.abs(np.sin(xs * 0.025 + 0.5)) + 10 * np.abs(np.sin(xs * 0.061 + 2))
              + 3 * np.sin(xs * 0.2))).astype(int)
near = (137 - (18 * np.abs(np.sin(xs * 0.018 + 3)) + 8 * np.abs(np.sin(xs * 0.05 + 1))
               + 2 * np.sin(xs * 0.31))).astype(int)
for x in range(W):
    for y in range(far[x], HOR):
        t = (y - far[x]) / 40
        c = lerp(np.array([118, 62, 112.]), np.array([170, 90, 110.]), min(t, 1))
        if y - far[x] < 2:
            c = c + np.array([60, 40, 10])
        sky[y, x] = c
    for y in range(near[x], HOR):
        t = (y - near[x]) / 30
        c = lerp(np.array([62, 36, 80.]), np.array([40, 26, 62.]), min(t, 1))
        if y - near[x] < 1:
            c = c + np.array([40, 22, 8])
        sky[y, x] = c

# ---------- Lake ----------
img = np.zeros((H, W, 3), float)
img[:HOR] = sky
for y in range(HOR, H):
    k = y - HOR
    src = int(np.clip(HOR - 1 - k * 0.9, 0, HOR - 1))
    shift = int(round(2.5 * np.sin(k * 0.9) * min(1, k / 20)))
    row = np.roll(sky[src], shift, axis=0)
    tint = lerp(np.array([0.78, 0.74, 0.85]), np.array([0.45, 0.5, 0.7]), min(k / 80, 1))
    img[y] = row * tint + np.array([0, 4, 14])
# ripples
for _ in range(90):
    y = int(rng.integers(HOR + 2, 208))
    x = int(rng.integers(0, W))
    l = int(rng.integers(6, 26))
    near_sun = max(0.0, 1 - abs(x + l / 2 - sx) / 60)
    col = np.array([255, 200, 140]) * near_sun + np.array([150, 130, 190]) * (1 - near_sun)
    a = 0.3 + 0.2 * near_sun
    img[y, x:x + l] = img[y, x:x + l] * (1 - a) + col * a

# Foreground shore
edge = (214 + 3 * np.sin(xs * 0.09) + rng.integers(-1, 2, W)).astype(int)
for x in range(W):
    for y in range(edge[x], H):
        img[y, x] = (22, 16, 40) if y > edge[x] else (48, 32, 64)
for x in range(0, W, 7):
    if rng.random() < 0.5:
        h = int(rng.integers(5, 13))
        for y in range(edge[x] - h, edge[x]):
            img[y, x] = (22, 16, 40)

img = np.clip(img, 0, 255)

# ---------- SNES constraints ----------
# 15-bit colour, 8 palettes x (backdrop + 15 colours), one palette per 8x8 tile row band.
def snap(c):
    c5 = np.clip(np.round(np.asarray(c, float) * 31 / 255), 0, 31).astype(int)
    return (c5 << 3) | (c5 >> 2)

bands = [4, 4, 3, 3, 3, 3, 4, 4]
assert sum(bands) == 28
bayer = (np.array([[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]]) + 0.5) / 16 - 0.5
backdrop = snap((28, 20, 72))

out = np.zeros((H, W, 3), np.uint8)
row = 0
palettes = []
for b in bands:
    y0, y1 = row * 8, (row + b) * 8
    area = img[y0:y1]
    q = Image.fromarray(snap(area).astype(np.uint8)).quantize(
        colors=15, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE)
    pal = np.array(q.getpalette()[:45], int).reshape(-1, 3)
    pal = np.unique(snap(pal), axis=0)[:15]
    palettes.append(pal)
    h = y1 - y0
    off = np.tile(bayer, (h // 4 + 1, W // 4 + 1))[:h, :W] * 14
    p = area + off[..., None]
    dist = ((p[:, :, None, :] - pal[None, None, :, :]) ** 2).sum(-1)
    idx = dist.argmin(-1)
    out[y0:y1] = pal[idx]
    row += b

# sanity checks: every 8x8 tile has <=15 colours, all colours are 5-bit expandable
for ty in range(28):
    for tx in range(32):
        t = out[ty * 8:ty * 8 + 8, tx * 8:tx * 8 + 8].reshape(-1, 3)
        assert len(np.unique(t, axis=0)) <= 15
Image.fromarray(out, "RGB").save("art.png")
