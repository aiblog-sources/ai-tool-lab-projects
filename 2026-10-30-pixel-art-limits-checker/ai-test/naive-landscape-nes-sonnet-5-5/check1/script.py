import numpy as np
import random
from PIL import Image

W, H = 256, 240
HOR = 144
rng = random.Random(7)

# Palette: index 0 is the shared backdrop; 4 sub-palettes of 3 colors each.
COLS = [
    (50, 30, 90),     # 0 N   backdrop / near mountains / lake
    (60, 40, 140),    # 1 S0
    (120, 40, 140),   # 2 S1
    (190, 40, 110),   # 3 S2
    (228, 80, 60),    # 4 S3
    (248, 140, 50),   # 5 S4
    (255, 240, 170),  # 6 SUN
    (110, 50, 120),   # 7 FAR
    (12, 8, 32),      # 8 T (trees / shore)
]
N, S0, S1, S2, S3, S4, SUN, FAR, T = range(9)
PALS = [{S0, S1, S2}, {S2, S3, S4}, {S4, SUN, FAR}, {T, FAR, S4}]

A = np.zeros((H, W), dtype=np.uint8)
yy, xx = np.mgrid[0:H, 0:W]
xs = np.arange(W)

# ---- Sky: bands aligned to the 16px grid, dither kept inside the band above
bounds = [(0, S0), (32, S1), (64, S2), (80, S3), (96, S4)]
for i, (b, c) in enumerate(bounds):
    e = bounds[i + 1][0] if i + 1 < len(bounds) else HOR
    A[b:e, :] = c
for i in range(1, len(bounds)):
    b, c = bounds[i]
    for j in range(4):
        y = b - 4 + j
        if j == 0:
            m = (xs % 4 == 0) & (y % 2 == 0)
        elif j == 1:
            m = (xs % 2 == 0) & (y % 2 == 1)
        elif j == 2:
            m = ((xs + y) % 2 == 0)
        else:
            m = ((xs + y) % 2 == 0) | (xs % 4 == 0)
        A[y, m] = c

# Stars
for _ in range(14):
    A[rng.randrange(2, 26), rng.randrange(W)] = S2

# Sun (half disc on the horizon)
sun_mask = ((xx - 128) ** 2 + (yy - HOR) ** 2 <= 34 ** 2) & (yy < HOR)
A[sun_mask] = SUN

# ---- Mountains
def ridge(peaks, step, lo):
    h = np.zeros(W)
    for cx, ph, w in peaks:
        h = np.maximum(h, ph - np.abs(xs - cx) * (ph / w))
    h = h.astype(int)
    j = 0
    for x in range(W):
        if x % step == 0:
            j = rng.choice([-2, -1, 0, 1, 2])
        h[x] += j
    return np.clip(h, lo, 46)

farh = ridge([(20, 40, 60), (85, 32, 55), (160, 44, 75), (228, 38, 65)], 4, 8)
nearh = ridge([(0, 26, 45), (62, 22, 45), (196, 28, 55), (250, 34, 45)], 4, 6)
for x in range(W):
    A[HOR - farh[x]:HOR, x] = FAR
    A[HOR - nearh[x]:HOR, x] = N

# ---- Lake
A[HOR:, :] = N
A[HOR, :] = FAR
for k in range(1, 26):
    if k % 2 == 1:
        off = [0, 1, 0, -1][k % 4]
        for x in range(W):
            if k <= farh[(x + off) % W] // 2:
                A[HOR + k, x] = FAR

# Sun glints (kept in the centre columns)
for k in range(2, 62, 2):
    y = HOR + k
    half = 4 + k // 3
    col = SUN if k < 30 else S4
    for x in range(128 - half, 128 + half + 1):
        if ((x // 3) + (k // 2)) % 2 == 0:
            A[y, x] = col

# Ripples
for _ in range(18):
    x = rng.randrange(0, W - 8)
    y = rng.randrange(HOR + 5, 100) * 2 if False else rng.randrange(HOR // 2 + 4, 100) * 2
    if HOR + 8 <= y <= 200:
        for i in range(rng.randint(3, 6)):
            A[y, x + i] = FAR

# ---- Shore
for x in range(W):
    top = 210 + ((x // 16) % 2) * 2
    if x < 64:
        top -= (64 - x) // 4
    if x >= 192:
        top -= (x - 191) // 4
    A[top:, x] = T

def pine(cx, base, h):
    for i in range(h):
        w = i // 3 + 1
        y = base - h + i
        for x in range(cx - w, cx + w + 1):
            if 0 <= x < W and 0 <= y < H:
                A[y, x] = T
    for y in range(base, base + 4):
        if y < H:
            A[y, cx] = T

for cx, b, h in [(10, 214, 46), (28, 218, 34), (46, 214, 24),
                 (236, 214, 50), (218, 218, 36), (206, 216, 24), (246, 220, 30)]:
    pine(cx, b, h)

# ---- Enforce NES attribute limits: pick a palette per 16x16 area and snap
RGB = np.array(COLS, dtype=int)
D = np.sqrt(((RGB[:, None, :] - RGB[None, :, :]) ** 2).sum(2))
nc = len(COLS)
pal_allowed = [sorted(p | {0}) for p in PALS]
remaps = []
for al in pal_allowed:
    remaps.append(np.array([min(al, key=lambda a: D[c, a]) for c in range(nc)],
                           dtype=np.uint8))
allm = [sum(1 << c for c in al) for al in pal_allowed]
pal_of = np.zeros((15, 16), dtype=int)
for ay in range(15):
    for ax in range(16):
        blk = A[ay * 16:ay * 16 + 16, ax * 16:ax * 16 + 16]
        hist = np.bincount(blk.ravel(), minlength=nc)
        costs = [(hist * D[np.arange(nc), rm]).sum() for rm in remaps]
        p = int(np.argmin(costs))
        pal_of[ay, ax] = p
        A[ay * 16:ay * 16 + 16, ax * 16:ax * 16 + 16] = remaps[p][blk]

# ---- Enforce tile budget by merging the rarest tiles into similar ones
BUDGET = 248

def tiles():
    d = {}
    for ty in range(30):
        for tx in range(32):
            k = A[ty * 8:ty * 8 + 8, tx * 8:tx * 8 + 8].tobytes()
            d.setdefault(k, []).append((ty, tx))
    return d

for _ in range(3000):
    d = tiles()
    if len(d) <= BUDGET:
        break
    keys = list(d)
    U = np.array([np.frombuffer(k, dtype=np.uint8) for k in keys])
    masks = [sum(1 << int(c) for c in set(u.tolist())) for u in U]
    order = sorted(range(len(keys)), key=lambda i: len(d[keys[i]]))[:20]
    best = None
    for i in order:
        pset = {int(pal_of[ty // 2, tx // 2]) for ty, tx in d[keys[i]]}
        ok = [j for j in range(len(keys)) if j != i
              and all((masks[j] & ~allm[p]) == 0 for p in pset)]
        if not ok:
            continue
        cost = D[U[i][None, :], U[ok]].sum(1)
        m = int(np.argmin(cost))
        score = cost[m] * len(d[keys[i]])
        if best is None or score < best[0]:
            best = (score, i, ok[m])
    if best is None:
        break
    _, i, j = best
    for ty, tx in d[keys[i]]:
        A[ty * 8:ty * 8 + 8, tx * 8:tx * 8 + 8] = U[j].reshape(8, 8)

out = RGB[A].astype(np.uint8)
Image.fromarray(out, "RGB").save("art.png")
