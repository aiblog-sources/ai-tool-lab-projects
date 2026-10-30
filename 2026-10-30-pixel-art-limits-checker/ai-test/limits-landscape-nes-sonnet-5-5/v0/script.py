import numpy as np
from PIL import Image
import math

W, H = 256, 240

NES = {
    0x03: (48, 0, 136), 0x05: (92, 0, 48), 0x0B: (0, 60, 0), 0x12: (48, 50, 236),
    0x14: (136, 20, 176), 0x15: (160, 20, 100), 0x1A: (8, 124, 0),
    0x25: (236, 88, 180), 0x26: (236, 106, 100), 0x27: (212, 136, 32),
    0x37: (228, 196, 144),
}
PALS = [
    [0x03, 0x14, 0x25, 0x26],  # sky top / dark water
    [0x03, 0x26, 0x27, 0x37],  # low sky, sun, reflection
    [0x03, 0x27, 0x05, 0x15],  # near mountains
    [0x03, 0x0B, 0x1A, 0x12],  # foreground
]

B = np.array([[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]])
thr = (B + 0.5) / 16.0


def dth(a, b, f, x, y):
    return b if f > thr[y % 4][x % 4] else a


code = np.full((H, W), 0x03, dtype=np.uint8)
fg = np.zeros((H, W), dtype=bool)

# ---------------- foreground mask/codes (computed first to decide palettes)
fgcode = np.zeros((H, W), dtype=np.uint8)


def ridge_fg(x):
    if x < 80:
        return 190 + (x / 80.0) * 42 + 3 * math.sin(x / 7.0)
    if x >= 176:
        return 232 - ((x - 176) / 80.0) * 44 + 3 * math.sin(x / 6.0)
    return None


for x in range(W):
    r = ridge_fg(x)
    if r is None:
        continue
    r = int(r)
    for y in range(r, H):
        d = y - r
        if d < 3:
            c = 0x1A
        elif d < 7:
            c = dth(0x1A, 0x0B, (d - 3) / 4.0, x, y)
        else:
            c = 0x0B
        fg[y, x] = True
        fgcode[y, x] = c


def tree(cx, h):
    base = int(ridge_fg(cx)) + 2
    top = base - h
    for y in range(top, base):
        hw = (y - top) / float(h) * h * 0.28
        for x in range(int(cx - hw), int(cx + hw) + 1):
            if 0 <= x < W and 0 <= y < H:
                c = 0x0B
                if x < cx - hw * 0.3 and (x + y) % 2 == 0:
                    c = 0x1A
                fg[y, x] = True
                fgcode[y, x] = c


for cx, h in [(12, 44), (34, 56), (56, 36), (204, 50), (228, 62), (247, 40)]:
    tree(cx, h)

# ---------------- block palettes
blockpal = np.zeros((15, 16), dtype=int)
for r in range(15):
    for c in range(16):
        if fg[r * 16:(r + 1) * 16, c * 16:(c + 1) * 16].any():
            p = 3
        elif r <= 4:
            p = 0
        elif r < 9:
            if r == 5 or 5 <= c <= 10:
                p = 1
            else:
                p = 2
        else:
            p = 1 if (5 <= c <= 10 and r <= 12) else 0
        blockpal[r, c] = p

# ---------------- sky
pairs = [(0x03, 0x14), (0x14, 0x14), (0x14, 0x25), (0x25, 0x26), (0x26, 0x26),
         (0x26, 0x27), (0x27, 0x27), (0x27, 0x27), (0x27, 0x27)]
for y in range(144):
    a, b = pairs[y // 16]
    f = ((y % 16) + 0.5) / 16.0
    for x in range(W):
        code[y, x] = dth(a, b, f, x, y)

for sx, sy in [(20, 6), (52, 18), (84, 9), (113, 24), (150, 5), (181, 15), (214, 8), (238, 22)]:
    code[sy, sx] = 0x25

# ---------------- sun
cx, cy, R = 128, 140, 26
for y in range(cy - R, 144):
    for x in range(cx - R, cx + R + 1):
        if (x - cx) ** 2 + (y - cy) ** 2 <= R * R:
            c = 0x37
            if y > 126 and (y - 126) % 6 < 2:
                c = 0x27
            code[y, x] = c

# ---------------- far mountains (backdrop colour silhouette)
for x in range(W):
    base = 112 + 18 * math.sin(x / 23.0) + 10 * math.sin(x / 9.0 + 1)
    w = max(0.0, 1 - abs(x - 128) / 55.0) ** 0.5
    ridge = int(base * (1 - w) + (137 + 3 * math.sin(x / 4.0)) * w)
    ridge = max(80, min(143, ridge))
    code[ridge:144, x] = 0x03

# ---------------- near mountains
def near(x, top, peakx, lit_left):
    if top >= 144:
        return
    for y in range(top, 144):
        d = y - top
        lit = (x >= peakx) if not lit_left else (x <= peakx)
        code[y, x] = dth(0x15, 0x05, d / 14.0, x, y) if lit else 0x05


for x in range(0, 80):
    near(x, int(100 + abs(x - 40) * 1.1), 40, False)
for x in range(176, W):
    near(x, int(104 + abs(x - 210) * 1.18), 210, True)

# ---------------- water
def hsh(tx, ty):
    return (tx * 7349 + ty * 9157 + tx * ty * 31) % 997


RIP = {
    1: [(3, range(1, 6))],
    2: [(5, range(2, 8))],
    3: [(1, range(0, 4)), (6, range(4, 8))],
}

for y in range(144, H):
    for x in range(W):
        p = blockpal[y // 16, x // 16]
        if p == 1:
            d = y - 144
            if (y % 4) < 2:
                hw = (26 - d * 0.22) * (0.75 + 0.25 * math.sin(y * 1.7))
                hw = int(round(hw / 4.0)) * 4
                dx = abs(x + 0.5 - 128)
                if hw > 0:
                    if dx < hw * 0.5:
                        code[y, x] = 0x37
                    elif dx < hw * 0.8:
                        code[y, x] = 0x27
                    elif dx < hw:
                        code[y, x] = 0x26
        elif p == 0:
            f = 0.5 * (1 - (y - 144) / 24.0)
            if f > 0:
                code[y, x] = dth(0x03, 0x14, f, x, y)

for ty in range(18, 30):
    for tx in range(32):
        p = blockpal[(ty * 8) // 16, (tx * 8) // 16]
        if p == 1:
            continue
        h = hsh(tx, ty)
        if ty <= 20:
            v = (h % 5) + 1 if h % 5 < 3 else 0
        else:
            v = (h % 9) + 1 if h % 9 < 3 else 0
        if v == 0:
            continue
        c = 0x12 if p == 3 else (0x25 if (ty <= 20 and v == 2) else 0x14)
        for ly, xs in RIP[v]:
            for lx in xs:
                code[ty * 8 + ly, tx * 8 + lx] = c

# ---------------- foreground on top
code[fg] = fgcode[fg]

# ---------------- map codes to palette indices
def nearest(c, pal):
    rc = NES[c]
    return min(range(4), key=lambda i: sum((a - b) ** 2 for a, b in zip(rc, NES[pal[i]])))


idx = np.zeros((H, W), dtype=np.uint8)
for y in range(H):
    for x in range(W):
        pal = PALS[blockpal[y // 16, x // 16]]
        c = int(code[y, x])
        idx[y, x] = pal.index(c) if c in pal else nearest(c, pal)

# ---------------- enforce <=256 unique tiles (pattern-table limit)
tiles = idx.reshape(30, 8, 32, 8).transpose(0, 2, 1, 3).reshape(960, 64)
uniq, inv, cnt = np.unique(tiles, axis=0, return_inverse=True, return_counts=True)
inv = inv.reshape(-1)
cnt = cnt.astype(int)
alive = np.ones(len(uniq), dtype=bool)
n = len(uniq)
while n > 256:
    cand = np.where(alive)[0]
    i = cand[np.argmin(cnt[cand])]
    others = cand[cand != i]
    dist = (uniq[others] != uniq[i]).sum(axis=1)
    j = others[np.argmin(dist)]
    inv[inv == i] = j
    cnt[j] += cnt[i]
    alive[i] = False
    n -= 1
tiles = uniq[inv]
idx = tiles.reshape(30, 32, 8, 8).transpose(0, 2, 1, 3).reshape(H, W)

# ---------------- render
img = np.zeros((H, W, 3), dtype=np.uint8)
for r in range(15):
    for c in range(16):
        pal = PALS[blockpal[r, c]]
        lut = np.array([NES[k] for k in pal], dtype=np.uint8)
        blk = idx[r * 16:(r + 1) * 16, c * 16:(c + 1) * 16]
        img[r * 16:(r + 1) * 16, c * 16:(c + 1) * 16] = lut[blk]

Image.fromarray(img, "RGB").save("art.png")
