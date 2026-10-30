#!/usr/bin/env python3
# SNES dungeon room background, 256x224, one BG layer (Mode 1, BG1, 4bpp).
#  - Colors are 15-bit BGR555. Every channel is a multiple of 8, from 0 to 248.
#  - There are 3 palettes of 16 entries. Entry 0 is the shared backdrop color (black).
#  - Each 8x8 tile uses exactly one palette. The script checks this before saving.
#  - The map is 32x28 = 896 tiles, under the 1024 tile limit. Unique tiles are counted.
# Output: indexed PNG. Pixel index = palette*16 + color, laid out like CGRAM.
import math, random
from PIL import Image

W, H = 256, 224
TW, TH = W // 8, H // 8
rng = random.Random(1337)

def c5(r, g, b):
    return (r // 8 * 8, g // 8 * 8, b // 8 * 8)

BLACK = (0, 0, 0)
# stone (walls)
S0, S1, S2, S3, S4, S5 = [c5(*v) for v in [(24,24,32),(48,48,56),(72,72,80),
                                            (96,96,104),(120,120,128),(152,152,160)]]
# floor flagstones (warm gray)
F0, F1, F2, F3, F4 = [c5(*v) for v in [(40,32,32),(64,56,48),(88,80,64),
                                        (112,104,88),(136,128,112)]]
M0, M1 = c5(48,72,40), c5(72,96,48)                     # moss
W0, W1, W2, W3, W4 = [c5(*v) for v in [(56,32,16),(88,48,24),(120,72,32),
                                        (152,96,48),(184,128,64)]]
GOLD = c5(200,160,64)
FD, FR, FO, FY, FW = [c5(*v) for v in [(128,24,8),(200,64,8),(240,128,16),
                                        (248,200,64),(248,248,184)]]
WARM1, WARM2 = c5(112,88,64), c5(144,112,72)

PALETTES = [
    [BLACK, S0, S1, S2, S3, S4, S5, F0, F1, F2, F3, F4, M0, M1],            # 0 room
    [BLACK, S0, S1, S2, S3, S4, S5, W0, W1, W2, W3, W4, GOLD],              # 1 door
    [BLACK, S0, S1, S2, S3, S4, S5, W0, W1, FD, FR, FO, FY, FW, WARM1, WARM2],  # 2 torch
]
for p in PALETTES:
    assert len(p) <= 16 and p[0] == BLACK

img = [[BLACK] * W for _ in range(H)]
def put(x, y, c):
    if 0 <= x < W and 0 <= y < H:
        img[y][x] = c

def h(*a):  # deterministic hash -> [0,1)
    v = 2166136261
    for n in a:
        v = ((v ^ (n & 0xffffffff)) * 16777619) & 0xffffffff
    return v / 2**32

# ---------------- floor ----------------
FX0, FX1, FY0, FY1 = 24, 232, 32, 200
for y in range(FY0, FY1):
    for x in range(FX0, FX1):
        row = (y - FY0) // 16
        off = 8 if row % 2 else 0
        col = (x - FX0 + off) // 16
        lx, ly = (x - FX0 + off) % 16, (y - FY0) % 16
        base = F3 if h(row, col) < 0.25 else F2
        light = F4 if base == F3 else F3
        if lx == 0 or ly == 0:
            c = F0
        elif lx == 1 or ly == 1:
            c = light
        elif lx == 15 or ly == 15:
            c = F1
        else:
            r = rng.random()
            c = F1 if r < 0.05 else (light if r < 0.08 else base)
        put(x, y, c)

# cracks
for _ in range(14):
    x = rng.randrange(FX0 + 4, FX1 - 4); y = rng.randrange(FY0 + 6, FY1 - 4)
    for _ in range(rng.randrange(4, 9)):
        if img[y][x] != F0:
            put(x, y, F0); put(x + 1, y, F1)
        x += rng.choice((-1, 0, 1)); y += rng.choice((0, 1))
        x = min(max(x, FX0), FX1 - 2); y = min(y, FY1 - 1)

# moss patches along mortar near corners
for (mx, my) in [(30, 190), (36, 186), (222, 192), (226, 40), (30, 60), (140, 194), (210, 110)]:
    for y in range(my - 5, my + 6):
        for x in range(mx - 6, mx + 7):
            if FX0 <= x < FX1 and FY0 <= y < FY1:
                d = math.hypot(x - mx, (y - my) * 1.3)
                if d < 6 and rng.random() < 0.9 - d / 7:
                    put(x, y, M0 if img[y][x] in (F0, F1) else (M1 if rng.random() < 0.5 else img[y][x]))

# wall shadows on floor
DARK = {F4: F3, F3: F2, F2: F1, F1: F0, F0: BLACK, M1: M0, M0: F0, BLACK: BLACK}
def darken(x, y, n=1):
    for _ in range(n):
        img[y][x] = DARK.get(img[y][x], img[y][x])
for y in range(FY0, FY0 + 6):
    for x in range(FX0, FX1):
        n = 2 if y < FY0 + 2 else 1
        if y >= FY0 + 4 and (x + y) % 2:
            continue
        darken(x, y, n)
for x in range(FX0, FX0 + 5):
    for y in range(FY0 + 6, FY1):
        n = 2 if x < FX0 + 2 else 1
        if x >= FX0 + 3 and (x + y) % 2:
            continue
        darken(x, y, n)

# ---------------- walls ----------------
def wall_top(x, y):
    """Top surface of a wall: big 16x16 blocks."""
    lx, ly = x % 16, y % 16
    if lx == 0 or ly == 0:
        return S1
    if lx == 1 or ly == 1:
        return S3
    r = rng.random()
    return S1 if r < 0.08 else (S3 if r < 0.12 else S2)

def brick(x, y, y0):
    row = (y - y0) // 8
    off = 8 if row % 2 else 0
    lx, ly = (x + off) % 16, (y - y0) % 8
    base = S2 if h(row, (x + off) // 16, 9) < 0.3 else S3
    if lx == 0 or ly == 0:
        return S0
    if ly == 1:
        return S5 if base == S3 else S4
    if ly == 7 or lx == 15:
        return S1 if base == S2 else S2
    r = rng.random()
    return S1 if r < 0.06 else (S4 if r < 0.1 else base)

# top wall
for y in range(0, 32):
    for x in range(W):
        if y < 6:
            c = wall_top(x, y + 2)
        elif y == 6:
            c = S5
        elif y == 7:
            c = S0
        elif y >= 30:
            c = S1 if y == 30 else S0
        else:
            c = brick(x, y, 6)
        put(x, y, c)

# side walls
for y in range(32, H):
    for x in list(range(0, 24)) + list(range(232, W)):
        inner = (x >= 16 and x < 24) or (x >= 232 and x < 240)
        if inner:
            k = x - 16 if x < 24 else 239 - x
            if (y - 32) % 8 == 0:
                c = S0
            elif k == 0 and x < 24:
                c = S4
            elif k == 0:
                c = S3
            else:
                c = S1 if k > 4 else S2
            if x < 24 and k == 7:
                c = S0
            if x >= 232 and k == 7:
                c = S0
        else:
            c = wall_top(x, y)
        put(x, y, c)

# bottom wall
for y in range(200, H):
    for x in range(24, 232):
        if y == 200:
            c = S0
        elif y == 201:
            c = S4
        else:
            c = wall_top(x, y)
        put(x, y, c)

# ---------------- door (palette 1) ----------------
DX0, DX1 = 112, 144
for y in range(4, 32):
    for x in range(DX0, DX1):
        jamb = x < DX0 + 4 or x >= DX1 - 4
        lintel = y < 8
        if jamb or lintel:
            lx = (x - DX0) if x < DX0 + 4 else (DX1 - 1 - x)
            if (y - 4) % 8 == 0 and not lintel:
                c = S0
            elif lintel and (x - DX0) % 8 == 0:
                c = S0
            elif y == 4 or lx == 0:
                c = S5
            elif lx == 3 or y == 7 and not jamb:
                c = S2
            else:
                c = S4
            put(x, y, c)
for y in range(8, 32):
    for x in range(DX0 + 4, DX1 - 4):
        px = (x - DX0 - 4) % 6
        if px == 0:
            c = W0
        elif px == 1:
            c = W3
        elif px == 5:
            c = W1
        else:
            c = W1 if h(x, y // 3, 5) < 0.12 else W2
        if y == 31:
            c = W0
        put(x, y, c)
# rounded top corners of the wood
for (x, y) in [(116, 8), (117, 8), (116, 9), (139, 8), (138, 8), (139, 9)]:
    put(x, y, S0)
# iron bands with rivets
for by in (13, 25):
    for x in range(DX0 + 4, DX1 - 4):
        put(x, by - 1, S4); put(x, by, S2); put(x, by + 1, S1)
    for x in range(DX0 + 6, DX1 - 4, 6):
        put(x, by, S5)
# ring handle
for a in range(16):
    t = a / 16 * 2 * math.pi
    put(int(round(132 + 2.5 * math.cos(t))), int(round(19 + 2.5 * math.sin(t))), GOLD)
put(132, 16, W4)

# ---------------- torches (palette 2) ----------------
WARMMAP = {S5: WARM2, S4: WARM2, S3: WARM1, S2: WARM1}
FLAME = [  # (y, half-width) top to bottom
    (1, 0), (2, 0), (3, 1), (4, 1), (5, 2), (6, 2), (7, 3), (8, 3), (9, 3), (10, 2), (11, 2)]

def torch(cx, flip):
    cy = 12
    for y in range(0, 32):
        for x in range(cx - 16, cx + 16):
            d = math.hypot(x - cx, (y - cy) * 1.1)
            if d < 15 and img[y][x] in WARMMAP:
                if d < 10 or (x + y) % 2 == 0:
                    img[y][x] = WARMMAP[img[y][x]]
    # wall plate
    for y in range(17, 24):
        for x in range(cx - 3, cx + 3):
            put(x, y, W1 if (x == cx - 3 or y == 17) else W0)
    put(cx - 2, 18, S5); put(cx + 1, 22, S5)
    # stick
    for y in range(12, 19):
        put(cx - 1, y, W1); put(cx, y, W0)
    # cup
    for x in range(cx - 3, cx + 3):
        put(x, 11, S4); put(x, 12, S2); put(x, 13, S1)
    put(cx - 3, 13, BLACK); put(cx + 2, 13, BLACK)
    # flame
    lean = -1 if flip else 1
    for y, hw in FLAME:
        sx = cx - 1 + (lean if y < 5 else 0)
        for x in range(sx - hw - 1, sx + hw + 2):
            d = abs(x - sx - 0.5)
            if d > hw + 1:
                continue
            if d > hw:
                c = FD
            elif d > hw - 1:
                c = FR
            elif d > hw - 2:
                c = FO
            else:
                c = FY
            put(x, y, c)
    for y in (7, 8, 9):
        put(cx - 1, y, FW); put(cx, y, FW if y > 7 else FY)
    put(cx - 1 + lean * 2, 0, FD)  # spark

torch(72, False)
torch(184, True)

# ---------------- tile -> palette map ----------------
tilepal = [[0] * TW for _ in range(TH)]
for ty in range(0, 4):
    for tx in range(14, 18):
        tilepal[ty][tx] = 1
    for tx in list(range(7, 11)) + list(range(21, 25)):
        tilepal[ty][tx] = 2

# ---------------- validate & export ----------------
out = Image.new("P", (W, H))
pix = out.load()
unique = set()
for ty in range(TH):
    for tx in range(TW):
        pal = PALETTES[tilepal[ty][tx]]
        lut = {c: i for i, c in enumerate(pal)}
        tile = []
        for y in range(ty * 8, ty * 8 + 8):
            for x in range(tx * 8, tx * 8 + 8):
                c = img[y][x]
                assert all(v % 8 == 0 and v <= 248 for v in c), c
                if c not in lut:
                    raise SystemExit(f"tile ({tx},{ty}) color {c} not in palette {tilepal[ty][tx]}")
                i = lut[c]
                tile.append(i)
                pix[x, y] = tilepal[ty][tx] * 16 + i
        t = tuple(tile)
        rows = [t[r * 8:(r + 1) * 8] for r in range(8)]
        hf = tuple(v for r in rows for v in r[::-1])
        vf = tuple(v for r in rows[::-1] for v in r)
        hv = tuple(v for r in rows[::-1] for v in r[::-1])
        if not ({t, hf, vf, hv} & unique):
            unique.add(t)
assert len(unique) <= 1024

flat = []
for p in range(8):
    pal = PALETTES[p] if p < len(PALETTES) else []
    for i in range(16):
        flat.extend(pal[i] if i < len(pal) else BLACK)
flat += [0] * (768 - len(flat))
out.putpalette(flat)
out.save("art.png")
print(f"saved art.png: {len(PALETTES)} palettes, {len(unique)} unique tiles (flip-reduced)")
