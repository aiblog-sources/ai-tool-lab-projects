from PIL import Image, ImageDraw
import random

W, H = 256, 240
HOR = 144
random.seed(7)

# NES-like palette
C = {
    "sky0": (60, 40, 140),
    "sky1": (120, 40, 140),
    "sky2": (190, 40, 110),
    "sky3": (228, 80, 60),
    "sky4": (248, 140, 50),
    "sky5": (252, 200, 80),
    "sky6": (252, 236, 150),
    "sun": (255, 248, 200),
    "far": (110, 50, 120),
    "farhi": (170, 70, 120),
    "near": (50, 30, 90),
    "nearhi": (100, 50, 110),
    "dark": (20, 12, 48),
    "tree": (12, 8, 32),
}

img = Image.new("RGB", (W, H), C["sky0"])
px = img.load()

# Sky bands with checker dithering at the boundaries
bands = [(0, "sky0"), (28, "sky1"), (56, "sky2"), (82, "sky3"),
         (106, "sky4"), (124, "sky5"), (136, "sky6")]
for y in range(HOR):
    idx = 0
    for i, (s, _) in enumerate(bands):
        if y >= s:
            idx = i
    col = C[bands[idx][1]]
    for x in range(W):
        px[x, y] = col
    # dither zone: first 4 rows of a band blend with previous
    if idx > 0:
        d = y - bands[idx][0]
        if d < 4:
            prev = C[bands[idx - 1][1]]
            for x in range(W):
                if d == 0 and (x + y) % 2 == 0:
                    px[x, y] = prev
                elif d == 1 and (x + y) % 2 == 0:
                    px[x, y] = prev if x % 4 else col
                elif d == 2 and (x % 2 == 0 and y % 2 == 0):
                    px[x, y] = prev
                elif d == 3 and (x % 4 == 0 and y % 2 == 0):
                    px[x, y] = prev

# Stars at the top
for _ in range(30):
    x, y = random.randrange(W), random.randrange(0, 40)
    px[x, y] = C["sky6"]

# Sun
d = ImageDraw.Draw(img)
sx, sy, sr = 128, 134, 26
for y in range(sy - sr, HOR):
    for x in range(sx - sr, sx + sr + 1):
        if (x - sx) ** 2 + (y - sy) ** 2 <= sr * sr:
            px[x, y] = C["sun"] if (x - sx) ** 2 + (y - sy) ** 2 <= 18 * 18 else C["sky6"]
        elif (x - sx) ** 2 + (y - sy) ** 2 <= (sr + 4) ** 2 and (x + y) % 2 == 0:
            px[x, y] = C["sky5"]

# Mountains
def ridge(peaks, base, width_scale):
    h = [0] * W
    for x in range(W):
        v = 0
        for (cx, ph, w) in peaks:
            t = ph - abs(x - cx) * (ph / w)
            if t > v:
                v = t
        # small jaggedness
        h[x] = int(v)
    return h

far_peaks = [(20, 52, 70), (80, 40, 60), (150, 58, 80), (215, 46, 70), (255, 38, 40)]
near_peaks = [(0, 40, 50), (55, 30, 55), (190, 36, 60), (245, 44, 55)]
far = ridge(far_peaks, 0, 1)
near = ridge(near_peaks, 0, 1)
# add jitter steps
j = 0
for x in range(W):
    if x % 3 == 0:
        j = random.choice([-2, -1, 0, 1, 2])
    far[x] += j
j = 0
for x in range(W):
    if x % 4 == 0:
        j = random.choice([-2, -1, 0, 1, 2])
    near[x] += j

for x in range(W):
    ft = HOR - max(far[x], 8)
    for y in range(ft, HOR):
        c = C["far"]
        if y - ft < 2 and x > 0 and far[x] > far[x - 1] - 0 and x < sx + 20:
            c = C["farhi"]
        px[x, y] = c
    nt = HOR - max(near[x], 6)
    for y in range(nt, HOR):
        c = C["near"]
        if y - nt < 2 and near[x] >= near[max(x - 1, 0)]:
            c = C["nearhi"]
        px[x, y] = c

# Lake: mirrored scene, darkened, with ripples
def darken(c):
    if c == C["near"]:
        return C["dark"]
    if c == C["far"]:
        return C["near"]
    return tuple(int(v * 0.7) for v in c)

for y in range(HOR, H):
    k = y - HOR
    src = HOR - 1 - int(k * 0.55)
    if src < 0:
        src = 0
    off = 0
    if k % 3 == 0:
        off = random.choice([-2, -1, 0, 1, 2]) if k > 4 else 0
    for x in range(W):
        c = px[(x + off) % W, src]
        px[x, y] = darken(c)

# Sun reflection glints
for y in range(HOR + 2, 210, 2):
    k = y - HOR
    half = 4 + k // 3
    for _ in range(3):
        cx = sx + random.randint(-half, half)
        ln = random.randint(4, 10)
        col = C["sky6"] if k < 30 else C["sky5"] if k < 50 else C["sky4"]
        for x in range(cx - ln // 2, cx + ln // 2):
            if 0 <= x < W:
                px[x, y] = col

# Light ripples elsewhere
for _ in range(40):
    x, y = random.randrange(W), random.randrange(HOR + 6, 205)
    for i in range(random.randint(3, 7)):
        if x + i < W:
            px[x + i, y] = C["sky2"]

# Shoreline foreground
for x in range(W):
    top = 208 + int(5 * (1 if (x // 24) % 2 else 0)) + (x // 8) % 3
    if x < 70:
        top -= max(0, (70 - x) // 5)
    if x > 190:
        top -= max(0, (x - 190) // 5)
    for y in range(max(top, 200), H):
        px[x, y] = C["tree"]

# Pine trees
def pine(cx, base, h):
    for i in range(h):
        w = i // 3 + 1
        y = base - h + i
        for x in range(cx - w, cx + w + 1):
            if 0 <= x < W and 0 <= y < H:
                px[x, y] = C["tree"]
    for y in range(base, base + 4):
        if y < H:
            px[cx, y] = C["tree"]

for cx, b, h in [(10, 214, 46), (28, 218, 34), (46, 214, 24), (236, 214, 50),
                 (218, 218, 36), (200, 216, 24), (246, 220, 30)]:
    pine(cx, b, h)

img.save("art.png")
