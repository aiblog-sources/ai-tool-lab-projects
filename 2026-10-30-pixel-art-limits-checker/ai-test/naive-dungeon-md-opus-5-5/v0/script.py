#!/usr/bin/env python3
"""Top-down dungeon room background for the Sega Mega Drive / Genesis.

320x224, one 16-colour palette whose channels fall on the 9-bit MD colour
grid (each channel is a multiple of 36, so 8 levels of 0..252).
Saved as an indexed PNG named art.png.
"""
import numpy as np
from PIL import Image

W, H = 320, 224
rng = np.random.default_rng(1987)

# ---- Palette: 16 entries, all on the Mega Drive 3-bit-per-channel grid ----
PAL = [
    (0, 0, 0),         # 0  black / backdrop
    (36, 36, 72),      # 1  stone darkest / mortar
    (72, 72, 108),     # 2  stone dark
    (108, 108, 144),   # 3  stone mid
    (144, 144, 180),   # 4  stone light
    (72, 72, 72),      # 5  floor dark
    (108, 108, 108),   # 6  floor mid
    (144, 144, 144),   # 7  floor light
    (72, 36, 0),       # 8  wood dark
    (144, 72, 36),     # 9  wood mid
    (180, 108, 36),    # 10 wood light
    (36, 36, 36),      # 11 iron
    (216, 36, 0),      # 12 flame red
    (252, 144, 0),     # 13 flame orange
    (252, 252, 108),   # 14 flame yellow
    (36, 72, 36),      # 15 moss
]

img = np.zeros((H, W), dtype=np.uint8)

CAP = 8           # thickness of the wall tops seen from above
FACE_Y0, FACE_Y1 = 8, 40   # front face of the top wall
SIDE = 16         # side wall width
BOT = 208         # bottom wall starts here

# ---------------- Wall tops (cap) ----------------
def cap_pixel(x, y):
    row = y // 8
    off = 8 if row % 2 else 0
    bx, by = (x + off) % 16, y % 8
    if by == 7 or bx == 15:
        return 1
    if by == 0 or bx == 0:
        return 3
    return 2

for y in range(H):
    for x in range(W):
        if y < CAP or x < SIDE or x >= W - SIDE or y >= BOT:
            img[y, x] = cap_pixel(x, y)

# dark inner edge where wall tops meet the floor/face
img[CAP:BOT, SIDE - 1] = 1
img[CAP:BOT, W - SIDE] = 1
img[BOT, SIDE:W - SIDE] = 1

# ---------------- Top wall front face (bricks) ----------------
for y in range(FACE_Y0, FACE_Y1):
    row = (y - FACE_Y0) // 8
    off = 8 if row % 2 else 0
    for x in range(SIDE, W - SIDE):
        bx, by = (x + off) % 16, (y - FACE_Y0) % 8
        if by == 7 or bx == 15:
            c = 1
        elif by == 0 or bx == 0:
            c = 4
        elif by == 6 or bx == 14:
            c = 2
        else:
            c = 3 if rng.random() > 0.08 else 2
        img[y, x] = c
# base of the wall: dark line
img[FACE_Y1 - 1, SIDE:W - SIDE] = 1

# ---------------- Floor flagstones ----------------
FX0, FX1, FY0, FY1 = SIDE, W - SIDE, FACE_Y1, BOT
stone_dark = rng.random((20, 20)) < 0.25
for y in range(FY0, FY1):
    for x in range(FX0, FX1):
        sx, sy = (x - FX0) // 16, (y - FY0) // 16
        fx, fy = (x - FX0) % 16, (y - FY0) % 16
        dark = stone_dark[sy, sx]
        if fx == 15 or fy == 15:
            c = 1
        elif fx == 0 or fy == 0:
            c = 6 if dark else 7
        elif fx == 14 or fy == 14:
            c = 5
        else:
            c = 5 if dark else 6
            if rng.random() < 0.04:
                c = 5 if c == 6 else 1
        img[y, x] = c

# cracks
for _ in range(10):
    cx = int(rng.integers(FX0 + 2, FX1 - 12))
    cy = int(rng.integers(FY0 + 10, FY1 - 6))
    for i in range(int(rng.integers(4, 9))):
        if FX0 <= cx < FX1 and FY0 <= cy < FY1:
            img[cy, cx] = 1
        cx += 1
        cy += int(rng.integers(-1, 2))

# moss in mortar near the walls
for _ in range(60):
    x = int(rng.integers(FX0, FX1))
    y = int(rng.integers(FY0, FY1))
    near_edge = x < FX0 + 24 or x > FX1 - 24 or y > FY1 - 20
    if near_edge and img[y, x] in (1, 5):
        img[y, x] = 15
        if x + 1 < FX1 and img[y, x + 1] in (1, 5):
            img[y, x + 1] = 15

# shadow cast by the top wall and side walls
darker = {7: 6, 6: 5, 5: 1, 15: 1}
for y in range(FY0, FY0 + 8):
    for x in range(FX0, FX1):
        if y < FY0 + 6 or (x + y) % 2 == 0:
            img[y, x] = darker.get(int(img[y, x]), img[y, x])
for y in range(FY0, FY1):
    for x in list(range(FX0, FX0 + 4)) + list(range(FX1 - 3, FX1)):
        if x in (FX0, FX0 + 1, FX1 - 1) or (x + y) % 2 == 0:
            img[y, x] = darker.get(int(img[y, x]), img[y, x])

# ---------------- Door ----------------
DX0, DX1 = 144, 176          # opening
FRX0, FRX1 = 140, 180        # frame
DY0 = 6
# stone frame
for y in range(DY0 - 2, FACE_Y1):
    for x in range(FRX0, FRX1):
        edge = x in (FRX0, FRX1 - 1) or y == DY0 - 2
        img[y, x] = 1 if edge else (4 if (x - FRX0 + (y // 6)) % 8 else 2)
# keystone
img[DY0 - 2:DY0 + 4, 157:163] = 3
img[DY0 - 2, 157:163] = 1
img[DY0 + 3, 157:163] = 2
# wooden planks with an arched top
acx, acy, ar = (DX0 + DX1 - 1) / 2, DY0 + 16, 16.5
for y in range(DY0 + 2, FACE_Y1):
    for x in range(DX0, DX1):
        if y < acy and (x - acx) ** 2 + (y - acy) ** 2 > ar ** 2:
            continue
        px = (x - DX0) % 8
        c = 8 if px == 0 else (10 if px == 1 else 9)
        if px in (4,) and (y * 7 + x) % 11 == 0:
            c = 8  # wood grain knots
        img[y, x] = c
    # dark outline on the arch
for y in range(DY0 + 2, int(acy)):
    for x in range(DX0, DX1):
        d = ((x - acx) ** 2 + (y - acy) ** 2) ** 0.5
        if ar - 1.2 < d <= ar:
            img[y, x] = 1
# iron bands with rivets
for by in (16, 31):
    img[by:by + 2, DX0:DX1] = 11
    for rx in range(DX0 + 3, DX1, 8):
        img[by, rx] = 7
# ring handle
for (x, y) in [(167, 23), (168, 22), (169, 23), (166, 24), (170, 24),
               (166, 25), (170, 25), (167, 26), (169, 26), (168, 26)]:
    img[y, x] = 11
img[22, 168] = 7
# threshold step on the floor
img[FY0:FY0 + 3, DX0 - 2:DX1 + 2] = 3
img[FY0 + 2, DX0 - 2:DX1 + 2] = 2
img[FY0 + 3, DX0 - 2:DX1 + 2] = 1

# ---------------- Torches ----------------
FLAME = [
    "...C...",
    "..CC...",
    "..CDC..",
    ".CDDC..",
    ".CDEDC.",
    ".CDEDC.",
    "CDEEEDC",
    "CDEEEDC",
    ".CDEDC.",
    "..CCC..",
]
FC = {"C": 12, "D": 13, "E": 14}
TORCHES = [88, 232]

# warm light: brighten surrounding stone by one step
stone_up = {1: 2, 2: 3, 3: 4}
floor_up = {5: 6, 6: 7, 1: 5}
for cx in TORCHES:
    cy = 14
    for y in range(0, FY0 + 24):
        for x in range(cx - 40, cx + 40):
            d = ((x - cx) ** 2 + ((y - cy) * 1.2) ** 2) ** 0.5
            c = int(img[y, x])
            if y < FY0:
                if d < 16 or (d < 24 and (x + y) % 2 == 0):
                    img[y, x] = stone_up.get(c, c)
            else:
                if d < 30 or (d < 38 and (x + y) % 2 == 0):
                    img[y, x] = floor_up.get(c, c)

for cx in TORCHES:
    # wall plate
    img[26:34, cx - 3:cx + 3] = 11
    img[26, cx - 3:cx + 3] = 2
    img[27:33, cx - 3] = 2
    img[28, cx - 2] = 7
    img[31, cx + 1] = 7
    # handle
    img[20:28, cx - 1:cx + 1] = 9
    img[20:28, cx] = 8
    # cup
    img[18:21, cx - 3:cx + 3] = 11
    img[18, cx - 3:cx + 3] = 2
    # flame
    for r, line in enumerate(FLAME):
        for i, ch in enumerate(line):
            if ch in FC:
                img[8 + r, cx - 3 + i] = FC[ch]
    # sparks
    img[5, cx + 2] = 13
    img[3, cx - 2] = 12

# ---------------- Save ----------------
out = Image.fromarray(img, mode="P")
flat = [v for rgb in PAL for v in rgb]
out.putpalette(flat + [0] * (768 - len(flat)))
out.save("art.png")
