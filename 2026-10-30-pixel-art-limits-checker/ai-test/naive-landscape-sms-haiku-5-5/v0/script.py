from PIL import Image
import math

W, H = 256, 192
HORIZON = 120
OUT = "art.png"

# 4x4 Bayer matrix for ordered dithering between SMS palette levels
BAYER = [
    [0, 8, 2, 10],
    [12, 4, 14, 6],
    [3, 11, 1, 9],
    [15, 7, 13, 5],
]


def dither(c, x, y):
    """Map a float RGB color to SMS-valid levels (0,85,170,255) with ordered dithering."""
    out = []
    thr = (BAYER[y % 4][x % 4] + 0.5) / 16.0
    for v in c:
        q = max(0.0, min(255.0, v)) / 85.0
        base = int(math.floor(q))
        frac = q - base
        level = base + 1 if frac > thr else base
        out.append(max(0, min(3, level)) * 85)
    return tuple(out)


def lerp(a, b, t):
    return tuple(a[i] + (b[i] - a[i]) * t for i in range(3))


def snap(c):
    return tuple(max(0, min(3, round(v / 85))) * 85 for v in c)


img = Image.new("RGB", (W, H), (0, 0, 0))
px = img.load()

# Palette anchors (all valid SMS colors)
SKY_TOP = (85, 0, 85)
SKY_MID = (170, 0, 85)
SKY_LOW = (255, 85, 0)
SKY_GLOW = (255, 170, 85)

WATER_NEAR = (255, 85, 0)
WATER_FAR = (85, 0, 85)

SUN_CORE = (255, 255, 170)
SUN_EDGE = (255, 170, 85)

FAR_MTN = (170, 85, 170)
NEAR_MTN = (85, 0, 85)
SHADOW = (0, 0, 0)

SUN_X, SUN_Y, SUN_R = 176, 108, 16


def far_ridge(x):
    return (80 + 18 * math.sin(x * 0.04 + 1.0)
            + 10 * math.sin(x * 0.11 + 2.0)
            + 5 * math.sin(x * 0.27))


def near_ridge(x):
    return (100 + 12 * math.sin(x * 0.06 + 4.0)
            + 6 * math.sin(x * 0.19 + 1.0))


# 1) Sky gradient (dithered), top to horizon
for y in range(HORIZON):
    t = y / float(HORIZON)
    if t < 0.5:
        base = lerp(SKY_TOP, SKY_MID, t / 0.5)
    elif t < 0.85:
        base = lerp(SKY_MID, SKY_LOW, (t - 0.5) / 0.35)
    else:
        base = lerp(SKY_LOW, SKY_GLOW, (t - 0.85) / 0.15)
    for x in range(W):
        px[x, y] = dither(base, x, y)

# 2) Lake: gradient from warm near-horizon to deep purple at the bottom
for y in range(HORIZON, H):
    t = (y - HORIZON) / float(H - HORIZON - 1)
    base = lerp(WATER_NEAR, WATER_FAR, t)
    for x in range(W):
        px[x, y] = dither(base, x, y)

# 3) Sun reflection streaks on the water, widening and thinning toward the viewer
for y in range(HORIZON, H):
    d = y - HORIZON
    half_w = 3 + d * 0.35
    if d < 4:
        draw_bright = True
    else:
        draw_bright = (d % 3) != 1
    if not draw_bright:
        continue
    for x in range(W):
        if abs(x - SUN_X) <= half_w:
            px[x, y] = SUN_CORE if abs(x - SUN_X) < half_w * 0.5 else SUN_EDGE

# 4) Sun disk (only above horizon; mountains will cover the lower part)
for y in range(max(0, SUN_Y - SUN_R), HORIZON):
    for x in range(max(0, SUN_X - SUN_R), min(W, SUN_X + SUN_R + 1)):
        dx, dy = x - SUN_X, y - SUN_Y
        d2 = dx * dx + dy * dy
        if d2 <= SUN_R * SUN_R:
            if d2 <= (SUN_R - 5) ** 2:
                px[x, y] = SUN_CORE
            else:
                px[x, y] = dither(SUN_EDGE, x, y)

# 5) Far mountain range
for x in range(W):
    top = int(round(far_ridge(x)))
    for y in range(max(0, top), HORIZON):
        px[x, y] = dither(FAR_MTN, x, y) if y == top else FAR_MTN

# 6) Near mountain range, darker, with a shaded side on the left slopes
for x in range(W):
    top = int(round(near_ridge(x)))
    for y in range(max(0, top), HORIZON):
        if y == top:
            px[x, y] = NEAR_MTN
        elif x % 2 == 0 and y - top < 3:
            px[x, y] = SHADOW
        else:
            px[x, y] = NEAR_MTN

# 7) Mountain reflections on the lake (dim, dithered, mirrored downward)
for x in range(W):
    top = int(round(near_ridge(x)))
    depth = HORIZON - top
    for k in range(1, min(depth, 10) + 1):
        y = HORIZON + k
        if y >= H:
            break
        if (y + x) % 2 == 0:
            px[x, y] = NEAR_MTN

# Snap everything to SMS levels as a final safety pass
for y in range(H):
    for x in range(W):
        px[x, y] = snap(px[x, y])

img.save(OUT)
