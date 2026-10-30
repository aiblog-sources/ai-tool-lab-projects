#!/usr/bin/env python3
# Top-down dungeon room for the Sega Master System, 256x192 (32x24 tiles).
# Hardware constraints this follows:
#   - Every colour is a valid SMS 6-bit colour (channels 0/85/170/255).
#   - The whole image uses one 16-colour background palette.
#   - It is built from repeating 8x8 tiles; the script counts the unique tiles
#     and asserts the count fits in the VRAM left beside the name table and SAT.
import numpy as np
from PIL import Image

W, H = 256, 192
PAL = [
    (0, 0, 0),        # 0 black / mortar / shadow
    (85, 85, 85),     # 1 dark grey (floor stone)
    (170, 170, 170),  # 2 light grey (stone highlight)
    (255, 255, 255),  # 3 white
    (85, 85, 170),    # 4 blue stone (wall face)
    (0, 0, 85),       # 5 deep blue (wall mortar / caps)
    (85, 0, 0),       # 6 dark brown (wood grooves)
    (170, 85, 0),     # 7 wood
    (255, 170, 85),   # 8 wood highlight
    (255, 0, 0),      # 9 flame red
    (255, 170, 0),    # 10 flame orange
    (255, 255, 0),    # 11 flame yellow / brass
    (170, 170, 255),  # 12 lit blue stone
    (0, 85, 0),       # 13 moss
    (170, 85, 85),    # 14 (spare)
    (255, 255, 170),  # 15 flame core
]
LEVELS = {0, 85, 170, 255}
assert len(PAL) <= 16 and all(c in LEVELS for col in PAL for c in col)

img = np.zeros((H, W), dtype=np.uint8)

# ---------------- wall caps (outer rim, top-down view of wall tops) -------------
cap = np.full((16, 16), 5, np.uint8)
cap[0, :] = 4
cap[:, 0] = 4
cap[15, :] = 0
cap[:, 15] = 0
cap[7, 8:] = 0
cap[8, 8:] = 4
cap[8:, 7] = 0
for y in range(0, H, 16):
    for x in range(0, W, 16):
        img[y:y + 16, x:x + 16] = cap

# ---------------- top wall face (bricks) ---------------------------------------
for y in range(8, 32):
    for x in range(16, 240):
        row = (y - 8) // 8
        ly = (y - 8) % 8
        lx = (x + (8 if row % 2 else 0)) % 16
        if ly == 7 or lx == 15:
            c = 5
        elif ly == 0 or lx == 0:
            c = 12
        else:
            c = 4
        img[y, x] = c
img[8, 16:240] = 0  # crisp edge between cap and face

# ---------------- floor slabs ---------------------------------------------------
def slab(variant):
    s = np.full((16, 16), 1, np.uint8)
    s[0, :] = 2
    s[:, 0] = 2
    s[15, :] = 0
    s[:, 15] = 0
    if variant == 1:  # crack
        for (yy, xx) in [(3, 4), (4, 5), (5, 5), (6, 6), (7, 7), (8, 7), (9, 8), (10, 9), (11, 9)]:
            s[yy, xx] = 0
    elif variant == 2:  # speckles
        for (yy, xx) in [(4, 10), (9, 4), (11, 12), (6, 7)]:
            s[yy, xx] = 2
        for (yy, xx) in [(5, 10), (10, 4), (12, 12)]:
            s[yy, xx] = 0
    elif variant == 3:  # moss in corner + chip
        for (yy, xx) in [(12, 1), (13, 1), (13, 2), (14, 1), (14, 2), (14, 3), (11, 1)]:
            s[yy, xx] = 13
        s[3, 11] = 0
        s[3, 12] = 0
        s[4, 12] = 2
    return s

slabs = [slab(v) for v in range(4)]
for sy in range(9):
    for sx in range(14):
        h = (sx * 7 + sy * 13 + (sx * sy) * 3) % 7
        v = {0: 0, 1: 1, 2: 0, 3: 2, 4: 0, 5: 3, 6: 0}[h]
        y0, x0 = 32 + sy * 16, 16 + sx * 16
        img[y0:y0 + 16, x0:x0 + 16] = slabs[v]

# shadows cast by the walls onto the floor
img[32:34, 16:240] = 0
for x in range(16, 240):
    if x % 2 == 0:
        img[34, x] = 0
img[32:176, 16:18] = 0
img[32:176, 238:240] = 0
for y in range(32, 176):
    if y % 2 == 0:
        img[y, 18] = 0
        img[y, 237] = 0

# bright rim on the inner edges of the wall caps
img[32:177, 15] = 12
img[32:177, 240] = 12
img[176, 15:241] = 12

# ---------------- door (centred in top wall) -----------------------------------
# stone frame
img[2:32, 110:146] = 2
img[2, 110:146] = 3
img[2:32, 110] = 3
img[2:32, 145] = 1
img[6, 110:146] = 1
for y in range(7, 32, 6):
    img[y, 110:114] = 1
    img[y, 142:146] = 1
# dark opening top
img[7:10, 114:142] = 0
# planks
for y in range(10, 34):
    for x in range(114, 142):
        lx = (x - 114) % 7
        if lx == 6:
            c = 6
        elif lx == 0:
            c = 8
        else:
            c = 7
        img[y, x] = c
# iron bands with rivets
for by in (14, 27):
    img[by:by + 2, 114:142] = 1
    img[by + 2, 114:142] = 0
    for rx in range(116, 142, 7):
        img[by, rx] = 2
# ring handle
img[20:23, 135:138] = 11
img[21, 136] = 7
img[23, 135:138] = 6
# threshold step on the floor
img[34:37, 112:144] = 2
img[34, 112:144] = 3
img[37, 112:144] = 0

# ---------------- torches -------------------------------------------------------
FLAME = [
    "...9...",
    "..99...",
    "..9A9..",
    ".9AA9..",
    ".9AYA9.",
    "9AYWYA9",
    "9AYWYA9",
    ".9AYA9.",
    "..999..",
]
FMAP = {"9": 9, "A": 10, "Y": 11, "W": 15}

def torch(cx):
    # warm light on surrounding wall bricks (dithered falloff)
    for y in range(9, 32):
        for x in range(cx - 14, cx + 15):
            d2 = (x - cx) ** 2 + (y - 14) ** 2
            if img[y, x] == 4:
                if d2 < 64 or (d2 < 144 and (x + y) % 2 == 0):
                    img[y, x] = 12
            elif img[y, x] == 5 and d2 < 36:
                img[y, x] = 4
    # flame
    for ry, row in enumerate(FLAME):
        for rx, ch in enumerate(row):
            if ch in FMAP:
                img[9 + ry, cx - 3 + rx] = FMAP[ch]
    # stick
    img[18:24, cx - 1:cx + 1] = 7
    img[18:24, cx + 1] = 6
    img[18, cx - 1:cx + 1] = 8
    # iron bracket
    img[23:28, cx - 3:cx + 3] = 1
    img[23, cx - 3:cx + 3] = 2
    img[28, cx - 3:cx + 3] = 0
    img[23:28, cx + 3] = 0
    img[25, cx - 2] = 2
    img[25, cx + 1] = 2

torch(60)
torch(196)

# ---------------- verify SMS limits --------------------------------------------
assert img.max() < 16
tiles = set()
for ty in range(24):
    for tx in range(32):
        tiles.add(img[ty * 8:ty * 8 + 8, tx * 8:tx * 8 + 8].tobytes())
n = len(tiles)
print("unique tiles:", n, "of 768 screen cells")
assert n <= 448, "too many unique tiles for SMS VRAM"

out = Image.fromarray(img, mode="P")
flat = [c for col in PAL for c in col]
out.putpalette(flat + [0] * (768 - len(flat)))
out.save("art.png")
print("saved art.png")
