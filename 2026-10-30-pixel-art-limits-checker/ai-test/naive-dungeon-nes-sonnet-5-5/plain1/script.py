from PIL import Image
import numpy as np
import random

W, H = 256, 240
rng = random.Random(7)

# ---- NES palette colours (2C02 RGB values) ----
K  = (0, 0, 0)         # $0F  universal backdrop
D  = (60, 60, 60)      # $2D
M  = (84, 84, 84)      # $00
L  = (152, 150, 152)   # $10
B1 = (60, 24, 0)       # $07
B2 = (120, 60, 0)      # $17
R  = (152, 34, 32)     # $16
O  = (212, 136, 32)    # $27
Y  = (228, 196, 144)   # $37
# Background palettes (backdrop K):
#  P0 stone: D, M, L   P1 door: B1, B2, L   P2 torch: R, O, Y
# Every colour change happens on 16x16 attribute-aligned blocks.

cv = np.zeros((H, W, 3), dtype=np.uint8)

def rect(a, x0, y0, x1, y1, c):
    a[y0:y1 + 1, x0:x1 + 1] = c

def put_tile(tx, ty, t):
    cv[ty * 8:ty * 8 + 8, tx * 8:tx * 8 + 8] = t

# ---------- Wall tiles (16x8 bricks = two 8x8 tiles) ----------
def wall_tile(kind, v):
    a = np.zeros((8, 8, 3), dtype=np.uint8)
    a[:, :] = M
    a[0, :] = K
    a[1, :] = L
    a[7, :] = D
    if kind == 'L':
        a[:, 0] = K
        if v == 1: a[4, 3] = D
        if v == 2: a[3, 5] = L
    else:
        a[2:8, 7] = D
        if v == 1: a[4, 4] = D
        if v == 2: a[3, 2] = L
    return a

wall_cache = {(k, v): wall_tile(k, v) for k in 'LR' for v in range(3)}
for ty in range(H // 8):
    for tx in range(W // 8):
        left = (tx % 2 == 0) if ty % 2 == 0 else (tx % 2 == 1)
        v = rng.choice([0, 0, 1, 2])
        put_tile(tx, ty, wall_cache[('L' if left else 'R', v)])

# ---------- Floor tiles (16x16 slabs = 4 tiles) ----------
floor_cache = {}
def floor_tile(pos, v, top, left, right):
    key = (pos, v, top, left, right)
    if key in floor_cache:
        return floor_cache[key]
    a = np.zeros((8, 8, 3), dtype=np.uint8)
    a[:, :] = D
    if pos in ('TL', 'TR'):
        a[0, :] = K
        a[1, 1:] = M if pos == 'TL' else M
        if pos == 'TR':
            a[1, :] = M
    if pos in ('TL', 'BL'):
        a[:, 0] = K
        a[1:, 1] = M
    if pos == 'TL':
        a[0, :] = K
        a[1, 1:] = M
    if v == 1:
        a[4, 5] = M
        a[5, 3] = M
    elif v == 2:
        a[3, 3] = K
        a[3, 4] = K
        a[4, 5] = K
        a[5, 5] = K
    if top:
        a[0:3, :] = K
        for x in range(8):
            if x % 2 == 0:
                a[3, x] = K
    if left:
        for y in range(8):
            for x in range(3):
                if (x + y) % 2 == 0:
                    a[y, x] = K
    if right:
        for y in range(8):
            for x in range(5, 8):
                if (x + y) % 2 == 0:
                    a[y, x] = K
    floor_cache[key] = a
    return a

slab_var = {}
for ty in range(6, 28):
    for tx in range(2, 30):
        sy, sx = (ty - 6) // 2, (tx - 2) // 2
        if (sy, sx) not in slab_var:
            slab_var[(sy, sx)] = rng.choice([0, 0, 0, 1, 2])
        v = slab_var[(sy, sx)]
        pos = ('T' if (ty - 6) % 2 == 0 else 'B') + ('L' if (tx - 2) % 2 == 0 else 'R')
        t = floor_tile(pos, v, ty == 6, tx == 2, tx == 29)
        put_tile(tx, ty, t)

# ---------- Door (32x48 at x=112, y=0, palette: K B1 B2 L) ----------
door = np.zeros((48, 32, 3), dtype=np.uint8)
rect(door, 0, 0, 1, 47, L)
rect(door, 30, 0, 31, 47, L)
rect(door, 0, 0, 31, 1, L)
rect(door, 2, 2, 2, 47, K)
rect(door, 29, 2, 29, 47, K)
rect(door, 2, 2, 29, 2, K)
rect(door, 3, 3, 28, 43, B2)
for px in range(3, 29, 8):
    rect(door, px, 3, px, 43, B1)
    for gy in range(6, 43, 7):
        k = (px // 8) % 3
        if px + 4 <= 28 and gy + k < 44:
            door[gy + k, px + 4] = B1
        if px + 5 <= 28 and gy + k + 1 < 44:
            door[gy + k + 1, px + 5] = B1
for by in (10, 32):
    rect(door, 3, by, 28, by + 3, K)
    rect(door, 3, by, 28, by, L)
    for rx in range(6, 29, 8):
        door[by + 2, rx] = L
rect(door, 10, 20, 12, 22, L)
door[21, 11] = K
rect(door, 19, 20, 21, 22, L)
door[21, 20] = K
rect(door, 0, 44, 31, 45, L)
rect(door, 0, 46, 31, 47, K)
cv[0:48, 112:144] = door

# ---------- Torches (16x32 niches, palette: K R O Y) ----------
def make_torch():
    a = np.zeros((32, 16, 3), dtype=np.uint8)
    cx, ty = 8, 18
    for y in range(0, ty + 1):
        for x in range(16):
            if (x + y) % 2 == 0 and (x - cx) ** 2 + (y - (ty - 6)) ** 2 < 8 ** 2:
                a[y, x] = R
    rect(a, cx - 1, ty + 2, cx + 1, 31, O)
    rect(a, cx + 1, ty + 2, cx + 1, 31, R)
    rect(a, cx - 3, ty, cx + 3, ty + 3, O)
    rect(a, cx - 3, ty + 3, cx + 3, ty + 3, R)
    flame = [
        (-1, 1, -12, -11, R),
        (-2, 2, -10, -8, R),
        (-3, 3, -8, -2, R),
        (-2, 2, -6, -1, O),
        (-1, 1, -4, -1, Y),
        (0, 0, -9, -7, O),
    ]
    for p, q, ya, yb, c in flame:
        rect(a, cx + p, ty + ya, cx + q, ty + yb, c)
    rect(a, cx - 3, ty - 2, cx + 3, ty - 1, R)
    rect(a, cx - 2, ty - 2, cx + 2, ty - 1, O)
    rect(a, cx - 1, ty - 2, cx + 1, ty - 1, Y)
    return a

torch = make_torch()
for tx0 in (64, 176):
    cv[0:32, tx0:tx0 + 16] = torch

# ---------- Outer border ----------
cv[0, :] = K
cv[H - 1, :] = K
cv[:, 0] = K
cv[:, W - 1] = K

Image.fromarray(cv, "RGB").save("art.png")
