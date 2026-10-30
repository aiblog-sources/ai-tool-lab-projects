#!/usr/bin/env python3
# Top-down NES dungeon room background, 256x240.
# Every 16x16 attribute block uses one of 4 sub-palettes (shared backdrop = black $0F).
from PIL import Image
import numpy as np

W, H = 256, 240

# NES master-palette entries (common 2C02 approximation)
NES = {
    0x0F: (0, 0, 0),
    0x00: (124, 124, 124),
    0x10: (188, 188, 188),
    0x20: (248, 248, 248),
    0x0C: (0, 64, 88),
    0x08: (80, 48, 0),
    0x18: (172, 124, 0),
    0x16: (248, 56, 0),
    0x28: (248, 184, 0),
}

PALS = [
    [0x0F, 0x0C, 0x00, 0x10],  # 0: floor  (mortar teal-dark, stone gray, highlight)
    [0x0F, 0x00, 0x10, 0x20],  # 1: walls
    [0x0F, 0x08, 0x18, 0x00],  # 2: door   (dark wood, light wood, iron/stone)
    [0x0F, 0x00, 0x16, 0x28],  # 3: torch  (wall gray, red flame, yellow core)
]

idx = np.zeros((H, W), dtype=np.uint8)        # colour index 0..3
attr = np.zeros((H // 16, W // 16), dtype=np.uint8)  # palette per 16x16 block

def h(a, b):
    return ((a * 73856093) ^ (b * 19349663)) & 0xFFFF

# Room geometry (all multiples of 16)
TOP, BOT, LEFT, RIGHT = 48, 208, 32, 224
DOOR = (112, 16, 144, 48)                 # x0,y0,x1,y1
TORCHES = [(64, 16), (176, 16)]           # 16x16 blocks

def is_floor(x, y):
    return LEFT <= x < RIGHT and TOP <= y < BOT

# ---------- walls ----------
def wall_px(x, y, highlights=True):
    row = y // 8
    ly = y % 8
    lx = (x + (row % 2) * 8) % 16
    if ly == 7 or lx == 15:
        return 0
    if highlights:
        if ly == 0 or lx == 0:
            return 2
        bx = (x + (row % 2) * 8) // 16
        if h(bx, row) % 5 == 0 and (lx, ly) in ((5, 3), (6, 3), (10, 5)):
            return 2
        if h(bx, row) % 4 == 1 and (lx, ly) in ((3, 5), (11, 2)):
            return 0
    return 1

# ---------- floor ----------
def floor_px(x, y):
    lx, ly = x % 16, y % 16
    tx, ty = x // 16, y // 16
    if lx == 15 or ly == 15:
        v = 1
    elif lx == 0 or ly == 0:
        v = 3
    else:
        v = 2
        k = h(tx, ty) % 6
        if k == 0 and lx == ly and 3 <= lx <= 9:
            v = 1                     # diagonal crack
        elif k == 1 and ly == 8 and 4 <= lx <= 10:
            v = 1
        elif k == 2 and (lx, ly) in ((4, 4), (10, 9), (11, 9)):
            v = 1                     # pits
    # shadow cast by walls
    if y - TOP < 4 and (x + y) % 2 == 0:
        v = 1 if v else 0
    if y - TOP < 2:
        v = 0
    if x - LEFT < 3 and (x + y) % 2 == 0:
        v = 1 if v else 0
    return v

for y in range(H):
    for x in range(W):
        if is_floor(x, y):
            idx[y, x] = floor_px(x, y)
        else:
            idx[y, x] = wall_px(x, y)

for by in range(H // 16):
    for bx in range(W // 16):
        attr[by, bx] = 0 if is_floor(bx * 16, by * 16) else 1

# ---------- door (palette 2) ----------
dx0, dy0, dx1, dy1 = DOOR
for y in range(dy0, dy1):
    for x in range(dx0, dx1):
        lx, ly = x - dx0, y - dy0
        # stone frame, rounded top corners
        frame = lx < 4 or lx >= 28 or ly < 4
        corner = (lx < 7 and ly < 7 and (lx - 7) ** 2 + (ly - 7) ** 2 > 9) or \
                 (lx > 24 and ly < 7 and (lx - 24) ** 2 + (ly - 7) ** 2 > 9)
        if frame or corner:
            v = 3
            if lx in (0, 31) or ly == 0 or (ly % 8 == 7 and (lx < 4 or lx >= 28)):
                v = 0
            idx[y, x] = v
            continue
        # wooden planks
        px = (lx - 4) % 6
        if px == 5:
            v = 1
        elif px == 0:
            v = 2
        else:
            v = 2 if (ly + (lx - 4) // 6 * 3) % 9 else 1  # wood grain
        if ly in (9, 10, 24, 25):                         # iron bands
            v = 3
            if px == 2 and ly in (9, 24):
                v = 0                                     # rivets
        if ly >= 30:
            v = 1                                         # threshold shadow
        if (lx, ly) in ((23, 16), (24, 16), (23, 17), (24, 17)):
            v = 3                                         # ring handle
        if (lx, ly) in ((22, 18), (25, 18), (23, 19), (24, 19)):
            v = 3
        idx[y, x] = v
for by in range(dy0 // 16, dy1 // 16):
    for bx in range(dx0 // 16, dx1 // 16):
        attr[by, bx] = 2

# ---------- torches (palette 3) ----------
flame_outer = [
    "......##........",
    ".....###........",
    ".....####.......",
    "....######......",
    "....######......",
    "...########.....",
    "...########.....",
    "....######......",
    ".....####.......",
]
flame_inner = [
    "................",
    "................",
    "......#.........",
    "......##........",
    ".....###........",
    ".....####.......",
    ".....####.......",
    "......##........",
    "................",
]
for tx, ty in TORCHES:
    for ly in range(16):
        for lx in range(16):
            x, y = tx + lx, ty + ly
            v = wall_px(x, y, highlights=False)
            # bowl (with black outline) and handle
            if 9 <= ly <= 10 and 3 <= lx <= 10:
                v = 1 if 4 <= lx <= 9 and ly == 9 else 0
            elif ly == 11 and 4 <= lx <= 9:
                v = 0
            elif 12 <= ly <= 15 and lx in (6, 7):
                v = 0
            # flame
            fy = ly
            if fy < len(flame_outer):
                o = flame_outer[fy][(lx - 1) % 16] if 0 <= lx - 1 < 16 else "."
                i = flame_inner[fy][(lx - 1) % 16] if 0 <= lx - 1 < 16 else "."
                if i == "#":
                    v = 3
                elif o == "#":
                    v = 2
            idx[y, x] = v
    attr[ty // 16, tx // 16] = 3

# ---------- palette resolve & save ----------
img = np.zeros((H, W, 3), dtype=np.uint8)
for y in range(H):
    for x in range(W):
        pal = PALS[attr[y // 16, x // 16]]
        img[y, x] = NES[pal[idx[y, x]]]

Image.fromarray(img, "RGB").save("art.png")
print("saved art.png")
