from PIL import Image
import numpy as np
import random

# NES background limits respected by this script:
#  - 256x240 pixels, colours taken from the NES master palette
#  - one shared backdrop colour + 4 background palettes of 3 colours each
#  - each 16x16 pixel block (attribute cell) uses a single palette
#  - at most 256 unique 8x8 tiles (one pattern table)
random.seed(7)

W, H = 256, 240
HOR = 144

NES = {
    0x03: (48, 0, 136),
    0x04: (68, 0, 100),
    0x0D: (0, 0, 0),
    0x14: (136, 20, 176),
    0x15: (160, 20, 100),
    0x26: (236, 106, 100),
    0x27: (212, 136, 32),
    0x30: (236, 238, 236),
    0x37: (228, 196, 144),
}
def rgb(i): return NES[i]

BD = 0x03
PALS = [
    [0x03, 0x14, 0x15, 0x26],   # upper sky
    [0x03, 0x27, 0x37, 0x30],   # horizon, sun, glints
    [0x03, 0x26, 0x14, 0x04],   # mountains against sky
    [0x03, 0x04, 0x14, 0x0D],   # lake and shore
]

SKY = [(0, 0x03), (28, 0x14), (56, 0x15), (82, 0x26), (106, 0x27), (128, 0x37)]
FAR, FARHI, NEAR, NEARHI, DARK = 0x14, 0x14, 0x04, 0x03, 0x0D

img = Image.new("RGB", (W, H), rgb(0x03))
px = img.load()

# Sky bands with ordered dithering at boundaries
for y in range(HOR):
    idx = 0
    for i, (s, _) in enumerate(SKY):
        if y >= s:
            idx = i
    col = rgb(SKY[idx][1])
    for x in range(W):
        px[x, y] = col
    if idx > 0:
        d = y - SKY[idx][0]
        prev = rgb(SKY[idx - 1][1])
        if d < 4:
            for x in range(W):
                if d == 0 and (x + y) % 2 == 0:
                    px[x, y] = prev
                elif d == 1 and (x + y) % 2 == 0 and x % 4:
                    px[x, y] = prev
                elif d == 2 and x % 2 == 0 and y % 2 == 0:
                    px[x, y] = prev
                elif d == 3 and x % 4 == 0 and y % 2 == 0:
                    px[x, y] = prev

# A few stars on a coarse grid
for _ in range(12):
    x, y = random.randrange(0, W // 8) * 8 + 3, random.randrange(0, 3) * 8 + 4
    px[x, y] = rgb(0x26)

# Sun
sx, sy, sr = 128, 134, 26
for y in range(sy - sr - 4, HOR):
    for x in range(sx - sr - 4, sx + sr + 5):
        r2 = (x - sx) ** 2 + (y - sy) ** 2
        if r2 <= 18 * 18:
            px[x, y] = rgb(0x30)
        elif r2 <= sr * sr:
            px[x, y] = rgb(0x37)
        elif r2 <= (sr + 4) ** 2 and (x + y) % 2 == 0:
            px[x, y] = rgb(0x27)

# Mountains
def ridge(peaks):
    h = [0] * W
    for x in range(W):
        v = 0
        for (cx, ph, w) in peaks:
            t = ph - abs(x - cx) * (ph / w)
            if t > v:
                v = t
        h[x] = int(v)
    return h

far = ridge([(20, 52, 70), (80, 40, 60), (150, 58, 80), (215, 46, 70), (255, 38, 40)])
near = ridge([(0, 40, 50), (55, 30, 55), (190, 36, 60), (245, 44, 55)])
j = 0
for x in range(W):
    if x % 8 == 0:
        j = random.choice([-2, 0, 2])
    far[x] += j
for x in range(W):
    if x % 8 == 4:
        j = random.choice([-2, 0, 2])
    near[x] += j

for x in range(W):
    ft = HOR - max(far[x], 8)
    for y in range(ft, HOR):
        px[x, y] = rgb(FAR)
    nt = HOR - max(near[x], 6)
    for y in range(nt, HOR):
        c = NEAR
        if y - nt < 2 and near[x] >= near[max(x - 1, 0)]:
            c = NEARHI
        px[x, y] = rgb(c)

# Lake: reflection of the scene, in lake palette colours
dark_set = {rgb(NEAR), rgb(NEARHI)}
far_set = {rgb(FAR)}
for y in range(HOR, H):
    k = y - HOR
    src = max(HOR - 1 - int(k * 0.55), 0)
    off = 0
    if k % 4 == 0 and k > 4:
        off = random.choice([-2, 0, 2])
    for x in range(W):
        c = px[(x + off) % W, src]
        if c in dark_set:
            nc = 0x0D
        elif c in far_set:
            nc = 0x04
        else:
            nc = 0x14 if k < 30 else (0x04 if k < 64 else 0x03)
        px[x, y] = rgb(nc)

# Sun reflection glints
for y in range(HOR + 2, 210, 2):
    k = y - HOR
    half = 4 + k // 3
    for _ in range(3):
        cx = sx + random.randint(-half, half)
        ln = random.randint(4, 10)
        col = 0x37 if k < 30 else 0x27
        for x in range(cx - ln // 2, cx + ln // 2):
            if 0 <= x < W:
                px[x, y] = rgb(col)

# Light ripples
for _ in range(25):
    x, y = random.randrange(W), random.randrange(HOR + 6, 205)
    for i in range(random.randint(3, 7)):
        if x + i < W:
            px[x + i, y] = rgb(0x14)

# Shoreline
for x in range(W):
    top = 208 + int(5 * (1 if (x // 24) % 2 else 0)) + (x // 8) % 3
    if x < 70:
        top -= max(0, (70 - x) // 5)
    if x > 190:
        top -= max(0, (x - 190) // 5)
    for y in range(max(top, 200), H):
        px[x, y] = rgb(0x0D)

# Pines
def pine(cx, base, h):
    for i in range(h):
        w = i // 3 + 1
        y = base - h + i
        for x in range(cx - w, cx + w + 1):
            if 0 <= x < W and 0 <= y < H:
                px[x, y] = rgb(0x0D)
    for y in range(base, base + 4):
        if y < H:
            px[cx, y] = rgb(0x0D)

for cx, b, h in [(10, 214, 46), (28, 218, 34), (46, 214, 24), (236, 214, 50),
                 (218, 218, 36), (200, 216, 24), (246, 220, 30)]:
    pine(cx, b, h)

# ---------------- NES constraint enforcement ----------------
arr = np.array(img).astype(np.int32)
pal_rgb = [np.array([rgb(c) for c in p], dtype=np.int32) for p in PALS]

idx = np.zeros((H, W), dtype=np.uint8)
blockpal = np.zeros((H // 16, W // 16), dtype=np.int32)
for by in range(H // 16):
    for bx in range(W // 16):
        blk = arr[by * 16:(by + 1) * 16, bx * 16:(bx + 1) * 16].reshape(-1, 3)
        best = None
        for p, pc in enumerate(pal_rgb):
            d = ((blk[:, None, :] - pc[None, :, :]) ** 2).sum(-1)
            e = d.min(1).sum()
            if best is None or e < best[0]:
                best = (e, p, d.argmin(1))
        blockpal[by, bx] = best[1]
        idx[by * 16:(by + 1) * 16, bx * 16:(bx + 1) * 16] = best[2].reshape(16, 16)

# Reduce to at most 256 unique tiles (pattern data = colour indices 0-3)
T = idx.reshape(H // 8, 8, W // 8, 8).transpose(0, 2, 1, 3).reshape(-1, 64)
U, inv, cnt = np.unique(T, axis=0, return_inverse=True, return_counts=True)
inv = inv.reshape(-1)
n = len(U)
if n > 256:
    D = (U[:, None, :] != U[None, :, :]).sum(2).astype(np.float64)
    np.fill_diagonal(D, np.inf)
    alive = np.ones(n, dtype=bool)
    cnt = cnt.astype(np.float64)
    parent = np.arange(n)
    while alive.sum() > 256:
        Dm = np.where(alive[None, :], D, np.inf)
        Dm[~alive, :] = np.inf
        cost = cnt[:, None] * Dm
        i, j = np.unravel_index(np.argmin(cost), cost.shape)
        parent[i] = j
        cnt[j] += cnt[i]
        alive[i] = False
    def root(i):
        while parent[i] != i:
            i = parent[i]
        return i
    rmap = np.array([root(i) for i in range(n)])
    T = U[rmap[inv]]
else:
    T = U[inv]

idx = T.reshape(H // 8, W // 8, 8, 8).transpose(0, 2, 1, 3).reshape(H, W)

# Render final image from indices + per-block palettes
out = np.zeros((H, W, 3), dtype=np.uint8)
for by in range(H // 16):
    for bx in range(W // 16):
        pc = pal_rgb[blockpal[by, bx]]
        sub = idx[by * 16:(by + 1) * 16, bx * 16:(bx + 1) * 16]
        out[by * 16:(by + 1) * 16, bx * 16:(bx + 1) * 16] = pc[sub]

Image.fromarray(out, "RGB").save("art.png")
