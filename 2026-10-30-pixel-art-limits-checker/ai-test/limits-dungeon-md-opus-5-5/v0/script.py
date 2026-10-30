#!/usr/bin/env python3
# Top-down dungeon room for Sega Mega Drive / Genesis, 320x224 (40x28 tiles).
# Constraints respected:
#  - Every color is an exact 9-bit MD color (3 bits per channel).
#  - 15 opaque colors total -> fits in ONE 16-entry palette (index 0 left as
#    transparent/backdrop), so every 8x8 tile trivially uses <=15 colors of one palette.
#  - At most 1120 unique tiles (40x28), well under available VRAM tile space.
import numpy as np
from PIL import Image

W, H = 320, 224
LV = [0, 36, 73, 109, 146, 182, 219, 255]  # MD 3-bit channel levels


def c(r, g, b):
    return (LV[r], LV[g], LV[b])


PAL = [
    c(0, 0, 0),  # 0 black / deep shadow
    c(1, 1, 2),  # 1 wall mortar / dark stone
    c(2, 2, 3),  # 2 stone mid-dark
    c(3, 3, 4),  # 3 stone mid
    c(4, 4, 5),  # 4 stone highlight
    c(2, 2, 2),  # 5 floor mortar / dark
    c(3, 3, 2),  # 6 floor base
    c(4, 4, 3),  # 7 floor highlight
    c(2, 1, 0),  # 8 dark wood
    c(4, 2, 1),  # 9 wood
    c(5, 3, 1),  # 10 light wood
    c(6, 1, 0),  # 11 flame red
    c(7, 4, 0),  # 12 flame orange
    c(7, 7, 3),  # 13 flame core
    c(5, 5, 5),  # 14 iron
]

img = np.zeros((H, W), dtype=np.uint8)


def h(x, y):
    v = (x * 73856093) ^ (y * 19349663) ^ 0x5bd1e995
    v = (v ^ (v >> 13)) * 1274126177
    return (v ^ (v >> 16)) & 0xFF


# ---------------- floor: flagstones ----------------
for y in range(H):
    for x in range(W):
        row = y // 16
        xo = x + (8 if row % 2 else 0)
        ly, lx = y % 16, xo % 16
        if ly == 0 or lx == 0:
            col = 5
        elif ly == 1 or lx == 1:
            col = 7
        elif ly == 15 or lx == 15:
            col = 5
        else:
            r = h(x, y)
            col = 5 if r < 14 else (7 if r > 244 else 6)
        img[y, x] = col

# floor shadow next to walls
for y in range(48, 54):
    for x in range(32, 288):
        if img[y, x] == 6 or (y < 51 and img[y, x] == 7):
            img[y, x] = 5
for y in range(48, 200):
    for x in list(range(32, 36)) + list(range(284, 288)):
        if img[y, x] == 6:
            img[y, x] = 5

# ---------------- walls ----------------
def wall_top(x, y):
    """Top-down view of wall top (large blocks)."""
    row = y // 16
    xo = x + (8 if row % 2 else 0)
    ly, lx = y % 16, xo % 16
    if ly == 15 or lx == 15:
        return 1
    if ly == 0 or lx == 0:
        return 3
    r = h(x + 7, y + 3)
    return 1 if r < 10 else (3 if r > 240 else 2)


for y in range(H):
    for x in range(W):
        if x < 32 or x >= 288 or y >= 200 or y < 8:
            img[y, x] = wall_top(x, y)

# inner edges of wall tops (dark lip)
img[8:200, 31] = 0
img[8:200, 288] = 0
img[200, 31:289] = 0
img[7, 0:W] = 1

# top wall face: bricks from y=8..47
for y in range(8, 48):
    yy = y - 8
    row = yy // 8
    for x in range(32, 288):
        xo = x + (8 if row % 2 else 0)
        ly, lx = yy % 8, xo % 16
        if ly == 7 or lx == 15:
            col = 1
        elif ly == 0:
            col = 4
        elif ly == 6:
            col = 2
        else:
            r = h(x, y + 99)
            col = 2 if r < 18 else 3
        img[y, x] = col
# base of wall face shadow line
img[47, 32:288] = 0

# ---------------- door ----------------
DX0, DX1 = 140, 180
for y in range(8, 48):
    for x in range(DX0, DX1):
        img[y, x] = 4 if (x - DX0) % 4 != 3 or y % 8 == 0 else 3
img[8:48, DX0] = 1
img[8:48, DX1 - 1] = 1
img[8, DX0:DX1] = 1
# opening with arched top
for y in range(12, 48):
    for x in range(145, 175):
        if y < 15 and (x < 145 + (15 - y) * 2 or x >= 175 - (15 - y) * 2):
            continue
        img[y, x] = 0
for y in range(13, 48):
    for x in range(146, 174):
        if y < 16 and (x < 146 + (16 - y) * 2 or x >= 174 - (16 - y) * 2):
            continue
        lx = (x - 146) % 7
        if lx == 6:
            col = 8
        elif lx == 0:
            col = 10
        else:
            col = 9 if h(x // 7, y // 3) > 40 else 8
        img[y, x] = col
for by in (20, 38):
    img[by, 146:174] = 14
    img[by + 1, 146:174] = 2
    for rx in (148, 157, 166, 171):
        img[by, rx] = 4
# handle ring
for y in range(27, 34):
    for x in range(164, 171):
        d = (x - 167) ** 2 + (y - 30) ** 2
        if 4 <= d <= 9:
            img[y, x] = 14
img[30, 167] = 0
# threshold
img[48:51, 144:176] = 4
img[50, 144:176] = 5

# ---------------- torches ----------------
for cx in (88, 232):
    # warm light on bricks around torch (dithered)
    for y in range(8, 48):
        for x in range(cx - 18, cx + 19):
            d = (x - cx) ** 2 + (y - 18) ** 2
            if img[y, x] == 3 and (d < 100 or (d < 260 and (x + y) % 2 == 0)):
                img[y, x] = 4
            elif img[y, x] == 2 and d < 120:
                img[y, x] = 3
    # wall plate
    img[30:38, cx - 3:cx + 4] = 0
    img[31:37, cx - 2:cx + 3] = 14
    img[34, cx] = 1
    # stick
    img[21:34, cx - 1:cx + 2] = 9
    img[21:34, cx + 1] = 8
    img[21:34, cx - 1] = 10
    # cup
    img[19:22, cx - 3:cx + 4] = 14
    img[21, cx - 3:cx + 4] = 2
    img[19, cx - 4] = 0
    img[19, cx + 4] = 0
    # flame
    for y in range(4, 20):
        for x in range(cx - 5, cx + 6):
            t = (y - 4) / 15.0
            half = 1.0 + 4.5 * t ** 0.8
            if y > 16:
                half = 4.5 - (y - 16) * 0.5
            dx = abs(x - cx + (1 if y < 9 else 0))
            if dx <= half:
                if dx <= half * 0.35 and y > 9:
                    img[y, x] = 13
                elif dx <= half * 0.7 and y > 6:
                    img[y, x] = 12
                else:
                    img[y, x] = 11
    # glow on floor beneath
    for y in range(48, 66):
        for x in range(cx - 14, cx + 15):
            d = (x - cx) ** 2 + ((y - 48) * 1.6) ** 2
            if d < 200 and img[y, x] == 5 and (x + y) % 2 == 0:
                img[y, x] = 6
            elif d < 120 and img[y, x] == 6 and (x + y) % 2 == 0:
                img[y, x] = 7

# ---------------- checks ----------------
used = np.unique(img)
assert len(used) <= 15, "too many colors"
tiles = set()
for ty in range(H // 8):
    for tx in range(W // 8):
        t = img[ty * 8:ty * 8 + 8, tx * 8:tx * 8 + 8]
        assert len(np.unique(t)) <= 15
        tiles.add(t.tobytes())
assert len(tiles) <= 1120
print(f"colors used: {len(used)}, unique tiles: {len(tiles)}")

rgb = np.array(PAL, dtype=np.uint8)[img]
Image.fromarray(rgb, "RGB").save("art.png")
