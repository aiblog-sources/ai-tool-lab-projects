from PIL import Image
import numpy as np

W, H = 256, 192
HOR = 112

# Master System palette: each channel is 0, 85, 170 or 255. 16 colors used.
pal = [
    (85, 0, 85),      # 0 sky top / dark
    (170, 0, 85),     # 1
    (255, 85, 0),     # 2
    (255, 170, 0),    # 3
    (255, 255, 85),   # 4
    (255, 255, 170),  # 5 sun / stars
    (170, 0, 170),    # 6
    (85, 0, 170),     # 7 far mountains
    (0, 0, 85),       # 8 near mountains
    (0, 0, 0),        # 9 foreground
    (255, 170, 170),  # 10 snow caps
]
while len(pal) < 16:
    pal.append((0, 0, 0))

bayer = np.array([[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]]) / 16.0

img = np.zeros((H, W), dtype=np.uint8)

# Sky gradient with ordered dithering
ramp = [0, 6, 1, 2, 3, 4]
for y in range(HOR):
    t = (y / (HOR - 1)) ** 1.2 * (len(ramp) - 1)
    lo = int(np.floor(t))
    hi = min(lo + 1, len(ramp) - 1)
    f = t - lo
    for x in range(W):
        img[y, x] = ramp[hi] if f > bayer[y % 4, x % 4] else ramp[lo]

# Stars in the upper sky
rng = np.random.RandomState(7)
for _ in range(35):
    sx = rng.randint(0, W)
    sy = rng.randint(0, 40)
    img[sy, sx] = 5

# Sun
cx, cy, r = 128, 98, 19
for y in range(HOR):
    for x in range(W):
        d = np.hypot(x - cx, y - cy)
        if d <= r:
            img[y, x] = 5 if d < r - 6 else 4
        elif d <= r + 7:
            # glow, dithered
            if (d - r) / 7 < bayer[y % 4, x % 4] * 0.9 and img[y, x] in (3, 4, 2):
                img[y, x] = 4 if img[y, x] != 4 else 5

# Far mountains
xs = np.arange(W)
valley = 0.25 + 0.75 * np.minimum(1, np.abs(xs - 128) / 75.0)
far = (28 + 16 * np.sin(xs * 0.035 + 1.0) + 9 * np.sin(xs * 0.09 + 2.0)
       + 4 * np.sin(xs * 0.23)) * valley
far_top = (HOR - far).astype(int)
for x in range(W):
    for y in range(far_top[x], HOR):
        img[y, x] = 7
        # snow caps on high peaks
        if far[x] > 38 and y < far_top[x] + 3 + (x % 3):
            img[y, x] = 10
        elif y < far_top[x] + 12 and (x + y) % 2 == 0 and far[x] > 30 and y > far_top[x] + 6:
            img[y, x] = 6

# Near mountains (sides only)
near = np.zeros(W)
for x in range(W):
    dl = max(0, 1 - x / 110.0)
    dr = max(0, 1 - (W - 1 - x) / 100.0)
    near[x] = (30 * dl ** 0.8 + 24 * dr ** 0.8) * (1 + 0.15 * np.sin(x * 0.25)) \
              + 4 * np.sin(x * 0.4) * (dl + dr > 0.05)
near_top = (HOR - near).astype(int)
for x in range(W):
    if near[x] > 1:
        for y in range(near_top[x], HOR):
            img[y, x] = 8

# Lake: mirrored reflection, darkened, with ripples
dark = {5: 4, 4: 3, 3: 2, 2: 1, 1: 6, 6: 0, 0: 0, 7: 8, 8: 9, 10: 7, 9: 9}
for y in range(HOR, H):
    dist = y - HOR
    sy = HOR - 1 - dist
    if sy < 0:
        sy = 0
    for x in range(W):
        off = int(round(np.sin(dist * 0.9 + x * 0.02) * (1 + dist / 25.0)))
        sx = min(W - 1, max(0, x + off))
        c = img[sy, sx]
        c = dark[int(c)]
        # extra darkening toward the viewer, dithered
        if dist > 20 and dist / 80.0 > bayer[y % 4, x % 4] + 0.25:
            c = dark[c]
        img[y, x] = c
    # horizontal ripple streaks
    if dist % 6 == 3:
        for x in range(W):
            if (x * 7 + y * 3) % 11 < 3 + dist // 20:
                img[y, x] = dark[int(img[y, x])]
    if dist % 8 == 5:
        for x in range(W):
            if (x * 5 + y) % 13 == 0 and img[y, x] in (3, 4):
                img[y, x] = 5

# Shoreline and pine trees in black
def tree(bx, by, h):
    for i in range(h):
        w = (i * 5) // (h * 2) + 1 if i > 0 else 0
        w = int(i * 0.45) + (1 if i > 0 else 0)
        for x in range(bx - w, bx + w + 1):
            if 0 <= x < W and 0 <= by - h + i < H:
                img[by - h + i, x] = 9
    for y in range(by, by + 4):
        if y < H:
            img[y, bx] = 9

for x in range(W):
    left = 172 + int(10 * np.sin(x * 0.07)) + int(max(0, 1 - x / 80.0) * -14) if x < 90 else H
    right = H
    if x > 175:
        right = 176 + int(5 * np.sin(x * 0.1)) + int((x - 175) * -0.2)
    top = min(left, right)
    for y in range(max(top, 0), H):
        img[y, x] = 9
    # grassy bottom strip across the whole screen
    for y in range(186, H):
        img[y, x] = 9

for bx, by, h in [(12, 176, 26), (28, 172, 20), (46, 170, 16), (232, 172, 28), (214, 175, 20), (246, 178, 18)]:
    tree(bx, by, h)

# Output
out = np.zeros((H, W, 3), dtype=np.uint8)
for i, c in enumerate(pal):
    out[img == i] = c
Image.fromarray(out, "RGB").save("art.png")
