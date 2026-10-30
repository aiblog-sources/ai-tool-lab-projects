import numpy as np
from PIL import Image

W, H_IMG = 320, 224
HOR = 136  # horizon line
L = [0, 36, 72, 109, 145, 182, 218, 255]  # Mega Drive 3-bit channel levels

def c(r, g, b):
    return (L[r], L[g], L[b])

rng = np.random.RandomState(7)
img = np.zeros((H_IMG, W, 3), np.uint8)

bayer = np.array([[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]]) / 16.0

sky = [c(1, 0, 2), c(2, 0, 3), c(3, 1, 3), c(4, 1, 3), c(5, 2, 3), c(6, 3, 2), c(7, 4, 2), c(7, 5, 1)]
lake = [c(0, 0, 2), c(1, 0, 2), c(1, 1, 3), c(2, 1, 3), c(3, 2, 3), c(4, 2, 3), c(5, 3, 3), c(6, 4, 2)]

def pick(pal, t, x, y):
    t = min(max(t, 0), len(pal) - 1.001)
    i = int(t + bayer[y % 4, x % 4])
    return pal[min(i, len(pal) - 1)]

# sky
for y in range(HOR):
    t = (y / HOR) ** 1.1 * (len(sky) - 1)
    for x in range(W):
        img[y, x] = pick(sky, t, x, y)

# lake (mirrored gradient)
for y in range(HOR, H_IMG):
    ym = min(2 * HOR - y, HOR - 1)
    t = (max(ym, 0) / HOR) ** 1.1 * (len(lake) - 1)
    for x in range(W):
        img[y, x] = pick(lake, t, x, y)

# sun
SX, SY, R = 218, 116, 26
sun_col = c(7, 7, 3)
sun_col2 = c(7, 6, 2)
for y in range(HOR):
    for x in range(W):
        d = (x - SX) ** 2 + (y - SY) ** 2
        if d <= R * R:
            if y > SY + 8 and (y - SY) % 4 == 3:
                continue  # slits near the bottom
            img[y, x] = sun_col if d < (R - 6) ** 2 else sun_col2

# clouds
cloud_top = c(7, 3, 2)
cloud_mid = c(6, 2, 2)
cloud_low = c(4, 1, 3)
for (cy, cx, w) in [(34, 60, 70), (50, 200, 90), (70, 110, 60), (24, 270, 40), (86, 270, 45), (62, 20, 40)]:
    for k in range(3):
        col = [cloud_top, cloud_mid, cloud_low][k]
        yy = cy + k * 2
        half = int(w / 2 - k * 4 + rng.randint(-3, 4))
        for x in range(cx - half, cx + half):
            if 0 <= x < W:
                edge = abs(x - cx) / max(half, 1)
                if edge < 0.95 - 0.05 * ((x * 7) % 3):
                    img[yy, x] = col
                    if k == 0 and edge < 0.5 and yy - 1 >= 0:
                        img[yy - 1, x] = cloud_top

# mountains
def ridge(base, amp, seed, rough):
    r = np.random.RandomState(seed)
    xs = np.arange(W)
    h = np.zeros(W)
    for f in range(1, 7):
        h += r.uniform(0.4, 1) / f ** rough * np.sin(xs * f * 0.018 + r.uniform(0, 6.28))
    h = h / np.abs(h).max()
    return (base - amp * h).astype(int)

layers = [
    (ridge(100, 22, 3, 0.8), c(4, 1, 3), c(3, 1, 3)),
    (ridge(112, 20, 11, 0.9), c(3, 1, 3), c(2, 1, 3)),
    (ridge(124, 14, 5, 1.0), c(2, 0, 2), c(1, 0, 2)),
]
refl_cols = [c(2, 1, 3), c(1, 1, 3), c(1, 0, 2)]

for li, (top, col, shade) in enumerate(layers):
    for x in range(W):
        for y in range(top[x], HOR):
            # light edge on ridge, shaded slope
            slope = top[min(x + 1, W - 1)] - top[x]
            k = col if (slope <= 0 or y - top[x] < 2) else shade
            if (x + y) % 4 == 0 and y - top[x] > 3 and slope > 0:
                k = shade
            img[y, x] = k

# mountain reflections with ripples
for y in range(HOR, H_IMG):
    d = y - HOR
    ym = HOR - 1 - d
    if ym < 60:
        break
    dx = int(round(2 * np.sin(y * 0.8) * (1 + d / 20)))
    for x in range(W):
        xs_ = min(max(x + dx, 0), W - 1)
        for li in range(3):
            if ym >= layers[li][0][xs_]:
                if (y + li) % 2 == 0 or d < 6:
                    img[y, x] = refl_cols[li]
                else:
                    img[y, x] = lake[1 + li // 2]
                break

# sun reflection streaks
glow1 = c(7, 5, 2)
glow2 = c(6, 3, 2)
for y in range(HOR + 1, H_IMG - 8):
    d = y - HOR
    half = int(R * 0.9 - d * 0.1)
    if half < 3:
        half = 3
    if (y % 3) == 0:
        continue
    wob = int(3 * np.sin(y * 0.7))
    for x in range(SX - half + wob, SX + half + wob):
        if 0 <= x < W and rng.rand() > d / 120.0:
            img[y, x] = glow1 if abs(x - SX - wob) < half * 0.6 else glow2

# lake ripple highlights
rip = c(5, 3, 3)
for _ in range(90):
    y = rng.randint(HOR + 4, H_IMG - 6)
    x = rng.randint(0, W - 12)
    ln = rng.randint(3, 10) + (y - HOR) // 14
    for i in range(ln):
        if x + i < W:
            img[y, x + i] = rip

# foreground shore with pines
shore = c(0, 0, 1)
shore2 = c(1, 0, 1)
for x in range(W):
    edge = int(H_IMG - 10 - 6 * np.sin(x * 0.03 + 1) * (1 if x < 110 else 0.2) - (0 if x < 110 else 0))
    if x > 120 and x < 270:
        edge = H_IMG - 3 + int(np.sin(x * 0.2))
    for y in range(max(edge, 0), H_IMG):
        img[y, x] = shore if (x + y) % 5 else shore2

def pine(px, py, h):
    for i in range(h):
        w = int(i * 0.35) + 1
        for x in range(px - w, px + w + 1):
            if 0 <= x < W and py - h + i >= 0:
                img[py - h + i, x] = shore
    for y in range(py, py + 4):
        if y < H_IMG:
            img[y, px] = shore

for (px, py, h) in [(14, 212, 44), (36, 214, 32), (58, 216, 52), (86, 214, 30), (104, 218, 24), (300, 220, 36), (314, 222, 28)]:
    pine(px, py, h)

# snap safety: ensure every color is on the MD palette
Image.fromarray(img, "RGB").save("art.png")
