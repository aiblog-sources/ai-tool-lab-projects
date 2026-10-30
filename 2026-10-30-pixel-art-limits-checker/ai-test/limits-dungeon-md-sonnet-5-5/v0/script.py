from PIL import Image
import numpy as np

W, H = 320, 224

# Mega Drive colour levels (3 bits per channel)
L = [0, 36, 73, 109, 146, 182, 219, 255]
pal_def = [
    (0, 0, 0),        # 0 mortar / darkest
    (1, 1, 1),        # 1 dark stone
    (2, 2, 2),        # 2
    (3, 3, 3),        # 3
    (4, 4, 4),        # 4 light stone
    (2, 1, 0),        # 5 dark wood
    (3, 2, 1),        # 6 wood
    (4, 3, 1),        # 7 light wood
    (5, 4, 2),        # 8 wood highlight
    (7, 6, 2),        # 9 flame core
    (7, 4, 0),        # 10 flame mid
    (6, 2, 0),        # 11 flame outer
    (1, 1, 2),        # 12 dark floor
    (2, 2, 3),        # 13 floor
    (3, 3, 4),        # 14 floor light
    (5, 5, 5),        # 15 metal / highlight
]
palette = [tuple(L[c] for c in p) for p in pal_def]

idx = np.zeros((H, W), np.uint8)

def hsh(a, b, c=0):
    return ((a * 73856093) ^ (b * 19349663) ^ (c * 83492791)) & 0xFFFF

# ---------- walls ----------
def brick(x, y):
    row = y // 8
    off = 8 if row % 2 else 0
    xx = x + off
    bx, by = xx % 16, y % 8
    if by == 0 or bx == 0:
        return 0
    bid = hsh((xx // 16) % 4, row % 3)
    base = [2, 3, 2, 3][bid % 4]
    if by == 1 or bx == 1:
        c = base + 1
    elif by == 7 or bx == 15:
        c = base - 1
    else:
        c = base
        if hsh(bx, by, bid % 4) % 11 == 0:
            c += 1 if bid % 2 else -1
    return max(1, min(4, c))

for y in range(H):
    for x in range(W):
        idx[y, x] = brick(x, y)

# ---------- floor ----------
FX0, FX1, FY0, FY1 = 24, 296, 88, 200
for y in range(FY0, FY1):
    for x in range(FX0, FX1):
        lx, ly = (x - FX0) % 16, (y - FY0) % 16
        sid = hsh((x - FX0) // 16 % 5, (y - FY0) // 16 % 4)
        base = [12, 13, 13, 14][sid % 4]
        if lx == 0 or ly == 0:
            c = 0
        elif lx == 1 or ly == 1:
            c = 14 if base < 14 else 15
        elif lx == 15 or ly == 15:
            c = 12
        else:
            c = base
            if hsh(lx, ly, sid % 4) % 13 == 0:
                c = 12 if base > 12 else 13
        # shadow beside walls
        if y < FY0 + 8 or x < FX0 + 8 or x >= FX1 - 8:
            if c in (13, 14, 15):
                c -= 1
        idx[y, x] = c

# ---------- door ----------
DX0, DX1, DY0, DY1 = 136, 184, 24, 88
# stone frame
for y in range(DY0, DY1):
    for x in range(DX0, DX1):
        idx[y, x] = 4 if (x < DX0 + 2 or y < DY0 + 2) else (1 if (x >= DX1 - 2) else 3)
        if x < DX0 + 1 or x >= DX1 - 1 or y < DY0 + 1:
            idx[y, x] = 0
# wooden planks
PX0, PX1, PY0 = 144, 176, 32
for y in range(PY0, DY1):
    for x in range(PX0, PX1):
        lx = (x - PX0) % 8
        if lx == 0:
            c = 5
        elif lx == 1:
            c = 8
        elif lx == 7:
            c = 5
        else:
            c = 6 if (hsh(x // 8, y // 6) % 3) else 7
            if lx in (3, 4) and hsh(x, y // 3) % 7 == 0:
                c = 5
        idx[y, x] = c
# top arch corners
for i in range(3):
    for j in range(3 - i):
        idx[PY0 + i, PX0 + j] = 0
        idx[PY0 + i, PX1 - 1 - j] = 0
# iron bands
for by in (40, 72):
    for y in range(by, by + 4):
        for x in range(PX0, PX1):
            idx[y, x] = 15 if y == by else (3 if y < by + 3 else 0)
    for x in range(PX0 + 2, PX1, 8):
        idx[by + 1, x] = 0
# handle
for y in range(58, 63):
    for x in range(168, 173):
        idx[y, x] = 15 if (x + y) % 2 else 3
idx[60, 170] = 0
# step in front of door
for y in range(88, 92):
    for x in range(DX0 - 4, DX1 + 4):
        idx[y, x] = 14 if y == 88 else 13

# ---------- torches ----------
def torch(cx, top):
    # glow on wall (checker dither)
    gy = top + 8
    for y in range(max(0, gy - 22), min(80, gy + 22)):
        for x in range(cx - 24, cx + 25):
            if (x + y) % 2 == 0 and (x - cx) ** 2 + (y - gy) ** 2 < 22 ** 2:
                if 1 <= idx[y, x] <= 3:
                    idx[y, x] += 1
    # bracket / stick
    for y in range(top + 12, top + 32):
        for x in range(cx - 2, cx + 2):
            idx[y, x] = 5 if x == cx - 2 else (6 if x == cx - 1 else (7 if x == cx else 5))
    # cup
    for y in range(top + 8, top + 13):
        for x in range(cx - 4, cx + 4):
            if y - (top + 8) < 4 or abs(x - cx + 0.5) < 3:
                idx[y, x] = 15 if y == top + 8 else 3
    idx[top + 12, cx - 4:cx + 4] = 0
    # flame
    for y in range(top - 8, top + 8):
        t = (y - (top - 8)) / 16.0  # 0 top .. 1 bottom
        hw = 1 + int(5 * np.sin(min(1, t * 1.2) * 3.14159 * 0.75)) if t < 0.95 else 2
        for x in range(cx - hw, cx + hw):
            d = abs(x - cx + 0.5)
            if t > 0.55 and d < hw * 0.35:
                c = 9
            elif t > 0.3 and d < hw * 0.7:
                c = 10
            else:
                c = 11
            idx[y, x] = c
    idx[top - 9, cx - 1:cx] = 11

torch(104, 36)
torch(216, 36)

# ---------- output ----------
rgb = np.zeros((H, W, 3), np.uint8)
for i, c in enumerate(palette):
    rgb[idx == i] = c

img = Image.fromarray(idx, "P")
flat = []
for c in palette:
    flat += list(c)
flat += [0] * (768 - len(flat))
img.putpalette(flat)
img.save("art.png")

tiles = {idx[ty*8:ty*8+8, tx*8:tx*8+8].tobytes() for ty in range(28) for tx in range(40)}
print("unique tiles:", len(tiles), "colors used:", len(np.unique(idx)))
