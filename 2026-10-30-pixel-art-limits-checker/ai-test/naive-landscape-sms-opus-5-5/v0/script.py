# Sunset over mountains and a lake, 256x192, Sega Master System style.
# Every color comes from the SMS 6-bit palette (channel levels 0/85/170/255).
# The image uses 14 colors, under the 16-color background palette limit.
import math
import random
import numpy as np
from PIL import Image

W, H = 256, 192
HZ = 120  # lake horizon, aligned to the 8x8 tile grid

# ---- palette (SMS-legal colors) ----
PAL = [
    (0, 0, 0),        # 0 black
    (0, 0, 85),       # 1 navy
    (85, 0, 85),      # 2 dark purple
    (170, 0, 85),     # 3 magenta
    (255, 85, 85),    # 4 salmon
    (255, 85, 0),     # 5 orange
    (255, 170, 0),    # 6 amber
    (255, 255, 85),   # 7 yellow
    (255, 255, 255),  # 8 white
    (170, 85, 170),   # 9 far mountain lit
    (85, 85, 170),    # 10 far mountain shade
    (85, 0, 170),     # 11 mid mountain lit
    (0, 0, 170),      # 12 blue (water)
    (0, 85, 170),     # 13 water highlight
]
BLACK, NAVY, DPURP, MAG, SALM, ORNG, AMBR, YELL, WHITE, FARL, FARS, MIDL, BLUE, HILI = range(14)

BAYER = np.array([[0, 8, 2, 10],
                  [12, 4, 14, 6],
                  [3, 11, 1, 9],
                  [15, 7, 13, 5]]) / 16.0 + 1 / 32.0

img = np.zeros((H, W), dtype=np.uint8)
rng = random.Random(7)

# ---- sky gradient with ordered dither ----
stops = [(0, NAVY), (22, DPURP), (46, MAG), (70, SALM),
         (90, ORNG), (106, AMBR), (120, YELL)]
for y in range(HZ):
    for i in range(len(stops) - 1):
        y0, c0 = stops[i]
        y1, c1 = stops[i + 1]
        if y0 <= y < y1:
            t = (y - y0) / (y1 - y0)
            for x in range(W):
                img[y, x] = c1 if t > BAYER[y % 4, x % 4] else c0
            break

# ---- sun with retro stripe cuts ----
SX, SY, SR = 128, 98, 22
for y in range(SY - SR, SY + SR + 1):
    for x in range(SX - SR, SX + SR + 1):
        d = math.hypot(x - SX, y - SY)
        if d <= SR:
            dy = y - SY
            if dy > 2:
                gap = 1 + dy // 7
                if (dy % 6) < min(gap, 4):
                    continue
            img[y, x] = WHITE if d < SR - 7 else YELL
        elif d <= SR + 1.5 and img[y, x] in (ORNG, SALM):
            img[y, x] = AMBR

# ---- cloud streaks ----
clouds = [(40, 34, 46, DPURP, MAG), (196, 28, 52, DPURP, MAG),
          (90, 58, 60, MAG, ORNG), (210, 66, 40, MAG, ORNG),
          (24, 78, 36, MAG, ORNG), (160, 84, 44, MAG, AMBR)]
for cx, cy, ln, body, rim in clouds:
    for row, frac in ((0, 0.55), (1, 1.0), (2, 0.8)):
        half = int(ln * frac / 2)
        off = (row - 1) * 5
        for x in range(cx - half + off, cx + half + off):
            if 0 <= x < W:
                img[cy + row, x] = rim if row == 2 else body

# ---- mountain profiles (midpoint displacement) ----
def ridge(n, base, rough, seed, peaks):
    r = random.Random(seed)
    pts = [base + r.uniform(-rough, rough) for _ in range(n + 1)]
    h = np.zeros(W)
    step = W / n
    for x in range(W):
        i = int(x / step)
        t = (x - i * step) / step
        h[x] = pts[i] * (1 - t) + pts[min(i + 1, n)] * t
    for px, py, pw in peaks:
        for x in range(W):
            h[x] = min(h[x], py + abs(x - px) * pw)
    for x in range(W):  # small jagged noise
        h[x] += r.choice((-1, 0, 0, 1))
    return h.astype(int)

def draw_range(h, lit, shade, bottom):
    for x in range(W):
        lo = max(x - 3, 0)
        hi = min(x + 3, W - 1)
        d = h[hi] - h[lo]
        facing = (x < SX and d < 0) or (x > SX and d > 0)
        for y in range(max(h[x], 0), bottom):
            img[y, x] = lit if facing else shade

far = ridge(16, 92, 6, 3, [(38, 52, 0.9), (92, 64, 1.1), (178, 48, 0.85), (236, 60, 1.0)])
draw_range(far, FARL, FARS, HZ)
mid = ridge(20, 106, 4, 11, [(14, 82, 1.2), (70, 88, 1.0), (206, 80, 1.15), (252, 90, 1.0)])
draw_range(mid, MIDL, DPURP, HZ)

# ---- near shoreline with conifers ----
shore = [112 + int(2 * math.sin(x * 0.05)) for x in range(W)]
for x in range(W):
    for y in range(shore[x], HZ):
        img[y, x] = NAVY
x = 0
while x < W:
    hgt = rng.randint(6, 14)
    if abs(x - SX) < 30:
        hgt = rng.randint(3, 6)  # keep the sun view open
    top = shore[min(x, W - 1)] - hgt
    for y in range(top, shore[min(x, W - 1)] + 1):
        half = (y - top) // 2
        for xx in range(x - half, x + half + 1):
            if 0 <= xx < W:
                img[y, xx] = BLACK if (y - top) % 3 != 2 or xx < x else NAVY
    x += rng.randint(4, 8)

# ---- lake: distorted, darkened reflection ----
dark = {BLACK: BLACK, NAVY: BLACK, DPURP: NAVY, MAG: DPURP, SALM: MAG,
        ORNG: MAG, AMBR: ORNG, YELL: AMBR, WHITE: YELL, FARL: DPURP,
        FARS: NAVY, MIDL: NAVY, BLUE: NAVY, HILI: BLUE}
scene = img[:HZ].copy()
for y in range(HZ, H):
    dy = y - HZ
    sy = HZ - 1 - dy
    amp = 1 + dy / 24
    off = int(round(amp * math.sin(dy * 1.3)))
    for x in range(W):
        sx = min(max(x + off, 0), W - 1)
        img[y, x] = dark[int(scene[sy, sx])]

# deepen the lake toward the bottom
for y in range(150, H):
    t = (y - 150) / (H - 150)
    for x in range(W):
        if t > BAYER[y % 4, x % 4] and img[y, x] not in (YELL, AMBR, WHITE):
            img[y, x] = NAVY if img[y, x] != BLACK else BLACK

# horizon glint line
for x in range(W):
    if abs(x - SX) < 40 and img[HZ, x] != BLACK:
        img[HZ, x] = AMBR if x % 4 else YELL

# sun shimmer streaks
for y in range(HZ + 2, H, 2):
    dy = y - HZ
    width = max(4, 30 - dy // 3)
    segs = rng.randint(2, 4)
    for _ in range(segs):
        cx = SX + rng.randint(-width, width)
        ln = rng.randint(2, max(3, width // 3))
        col = YELL if dy < 25 and rng.random() < 0.6 else AMBR if dy < 50 else ORNG
        for xx in range(cx - ln // 2, cx + ln // 2 + 1):
            if 0 <= xx < W:
                img[y, xx] = col

# cool ripple highlights away from the sun
for y in range(HZ + 4, H, 3):
    for _ in range(5):
        cx = rng.randint(0, W - 1)
        if abs(cx - SX) < 40:
            continue
        ln = rng.randint(3, 8)
        col = HILI if y > 150 else MAG
        for xx in range(cx, min(cx + ln, W)):
            if img[y, xx] != BLACK:
                img[y, xx] = col

# ---- report hardware-relevant stats ----
tiles = set()
for ty in range(0, H, 8):
    for tx in range(0, W, 8):
        tiles.add(img[ty:ty + 8, tx:tx + 8].tobytes())
print("colors used:", len(np.unique(img)), "unique tiles:", len(tiles))

pal = np.array(PAL, dtype=np.uint8)
Image.fromarray(pal[img], "RGB").save("art.png")
