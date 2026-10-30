#!/usr/bin/env python3
# NES dungeon room background, 256x240, one background layer.
# Hardware limits followed:
#  - One shared backdrop color (black $0F) and 4 background palettes of 3 colors each
#  - One palette per 16x16 attribute area
#  - Each 8x8 tile uses at most 4 color indices
#  - Fewer than 256 unique tile patterns (checked by assert below)
import numpy as np
from PIL import Image

W, H = 256, 240

# Approximate 2C02 RGB values for the NES colors used here
NES = {
    0x0F: (0, 0, 0),
    0x2D: (79, 79, 79),
    0x00: (102, 102, 102),
    0x10: (173, 173, 173),
    0x07: (106, 36, 0),
    0x17: (161, 70, 0),
    0x08: (66, 52, 0),
    0x27: (237, 151, 48),
    0x38: (255, 230, 160),
}
PALS = [
    [0x0F, 0x00, 0x10, 0x2D],  # P0 walls: mortar, brick, highlight, shadow
    [0x0F, 0x00, 0x07, 0x17],  # P1 door: black, stone, dark wood, wood
    [0x0F, 0x00, 0x27, 0x38],  # P2 torch: black, stone, orange, yellow
    [0x0F, 0x2D, 0x00, 0x08],  # P3 floor: black, stone, highlight, dirt
]

idx = np.zeros((H, W), dtype=np.uint8)
pal = np.full((15, 16), 3, dtype=np.uint8)


def hsh(a, b):
    return ((a * 73856093) ^ (b * 19349663) ^ (a * b * 83492791)) & 0xFFFF


def is_wall(x, y):
    return y < 48 or y >= 208 or x < 32 or x >= 224


# Palette assignment for each 16x16 attribute area
for my in range(15):
    for mx in range(16):
        if is_wall(mx * 16, my * 16):
            pal[my, mx] = 0
for my in (1, 2):
    pal[my, 7] = pal[my, 8] = 1  # door
    pal[my, 4] = pal[my, 11] = 2  # torches

WALL_VAR = {
    1: ([(4, 2), (5, 3), (5, 4), (6, 5)], 3),
    2: ([(10, 3), (3, 5), (12, 2)], 3),
    3: ([(8, 2), (9, 2), (9, 3)], 2),
}
FLOOR_VAR = {
    1: ([(5, 4), (6, 5), (6, 6), (7, 7), (8, 7), (9, 8), (9, 9)], 0),
    2: ([(3, 10), (4, 11), (11, 4), (12, 12)], 3),
    3: ([(10, 3), (10, 4), (11, 5), (12, 6)], 0),
}

# Base pass: walls and floor
for y in range(H):
    for x in range(W):
        p = pal[y // 16, x // 16]
        if is_wall(x, y):
            row = y // 8
            off = (row % 2) * 8
            bx = (x + off) // 16
            lx, ly = (x + off) % 16, y % 8
            if ly == 7 or lx == 15:
                v = 0
            elif ly == 0 or lx == 0:
                v = 2
            elif ly == 6 or lx == 14:
                v = 3
            else:
                v = 1
                var = hsh(bx, row) % 7
                if var in WALL_VAR and (lx, ly) in WALL_VAR[var][0]:
                    v = WALL_VAR[var][1]
            if p != 0:
                v = 0 if v == 0 else 1
            idx[y, x] = v
        else:
            row = y // 16
            off = (row % 2) * 8
            bx = (x + off) // 16
            lx, ly = (x + off) % 16, y % 16
            if lx == 15 or ly == 15:
                v = 0
            elif lx == 0 or ly == 0:
                v = 2
            else:
                v = 1
                var = hsh(bx + 7, row + 3) % 6
                if var in FLOOR_VAR and (lx, ly) in FLOOR_VAR[var][0]:
                    v = FLOOR_VAR[var][1]
            idx[y, x] = v

# Wall inner edges (dark line on the wall side)
for x in range(32, 224):
    if 112 <= x < 144:
        continue
    idx[47, x] = 3 if pal[2, x // 16] == 0 else 0
    idx[208, x] = 3
for y in range(48, 208):
    idx[y, 31] = 3
    idx[y, 224] = 3

# Floor shadows cast by the walls
for x in range(32, 224):
    idx[48, x] = 0
    if (x + 49) % 2 == 0:
        idx[49, x] = 0
for y in range(48, 208):
    idx[y, 32] = 0
    idx[y, 223] = 0
    if (y + 33) % 2 == 0:
        idx[y, 33] = 0
    if (y + 222) % 2 == 0:
        idx[y, 222] = 0

# Door (palette P1): x 112..143, y 16..47
DX, DY = 112, 16
cx, cy = 15.5, 14.0


def door_outer(lx, ly):
    return ly >= 14 or (lx - cx) ** 2 + (ly - cy) ** 2 <= 16.2 ** 2


def door_open(lx, ly):
    if not (4 <= lx <= 27):
        return False
    return ly >= 14 or (lx - cx) ** 2 + (ly - cy) ** 2 <= 12.0 ** 2


for ly in range(32):
    for lx in range(32):
        x, y = DX + lx, DY + ly
        if not door_outer(lx, ly):
            continue
        if door_open(lx, ly):
            # wood planks
            if (lx - 4) % 6 == 5:
                v = 0
            elif (lx - 4) % 6 == 0:
                v = 2
            else:
                v = 3
                if (lx * 3 + ly * 7) % 11 == 0:
                    v = 2
            if ly in (9, 24):
                v = 0
                if (lx - 4) % 6 == 2:
                    v = 1  # bolts
            idx[y, x] = v
        else:
            # stone frame
            edge = False
            for ddx, ddy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                nx, ny = lx + ddx, ly + ddy
                if 0 <= nx < 32 and 0 <= ny < 32:
                    if not door_outer(nx, ny) or door_open(nx, ny):
                        edge = True
                elif ny < 0 or nx < 0 or nx >= 32:
                    edge = True
            v = 0 if edge else 1
            if ly >= 14 and ly % 6 == 1:
                v = 0
            if ly < 14 and abs(lx - cx) < 0.6:
                v = 0  # keystone seam
            idx[y, x] = v
# door ring handle
for (lx, ly) in [(22, 17), (23, 17), (21, 18), (24, 18), (21, 19), (24, 19), (22, 20), (23, 20)]:
    idx[DY + ly, DX + lx] = 1
# darker threshold under the door opening
for lx in range(4, 28):
    idx[DY + 31, DX + lx] = 2

# Torches (palette P2)
OUTER = {1: (7, 7), 2: (7, 8), 3: (6, 8), 4: (6, 9), 5: (5, 9), 6: (5, 10), 7: (4, 10),
         8: (4, 11), 9: (4, 11), 10: (4, 11), 11: (4, 11), 12: (5, 10), 13: (5, 10), 14: (6, 9)}
INNER = {5: (7, 7), 6: (7, 8), 7: (6, 8), 8: (6, 9), 9: (6, 9), 10: (5, 9),
         11: (6, 10), 12: (6, 9), 13: (7, 8)}


def draw_torch(ox, oy):
    flame = set()
    for ly, (a, b) in OUTER.items():
        for lx in range(a, b + 1):
            flame.add((lx, ly))
    flame.add((9, 0))
    flame.add((9, 1))
    # black outline around flame
    for (lx, ly) in list(flame):
        for ddx, ddy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            n = (lx + ddx, ly + ddy)
            if n not in flame and 0 <= n[0] < 16 and 0 <= n[1] < 32:
                idx[oy + n[1], ox + n[0]] = 0
    for (lx, ly) in flame:
        idx[oy + ly, ox + lx] = 2
    for ly, (a, b) in INNER.items():
        for lx in range(a, b + 1):
            idx[oy + ly, ox + lx] = 3
    # cup
    cup = {15: (3, 12), 16: (4, 11), 17: (5, 10), 18: (6, 9)}
    for ly, (a, b) in cup.items():
        for lx in range(a, b + 1):
            idx[oy + ly, ox + lx] = 0 if lx in (a, b) or ly == 18 else 1
    for lx in range(5, 11):
        idx[oy + 15, ox + lx] = 3  # glowing rim
    # handle
    for ly in range(19, 29):
        idx[oy + ly, ox + 6] = 0
        idx[oy + ly, ox + 9] = 0
        idx[oy + ly, ox + 7] = 2
        idx[oy + ly, ox + 8] = 2 if ly % 3 else 0
    # wall bracket
    for ly in range(23, 27):
        for lx in range(3, 13):
            edge = ly in (23, 26) or lx in (3, 12)
            idx[oy + ly, ox + lx] = 0 if edge else 1
    idx[oy + 24, ox + 4] = 0
    idx[oy + 24, ox + 11] = 0
    for lx in range(6, 10):
        idx[oy + 29, ox + lx] = 0


draw_torch(64, 16)
draw_torch(176, 16)

# ---- Hardware checks ----
tiles = set()
for ty in range(30):
    for tx in range(32):
        tiles.add(idx[ty * 8:ty * 8 + 8, tx * 8:tx * 8 + 8].tobytes())
assert len(tiles) <= 256, f"too many unique tiles: {len(tiles)}"
assert idx.max() <= 3
all_colors = {c for p in PALS for c in p}
assert len({p[0] for p in PALS}) == 1  # shared backdrop
assert len(all_colors) <= 13

# ---- Render ----
img = np.zeros((H, W, 3), dtype=np.uint8)
for y in range(H):
    for x in range(W):
        img[y, x] = NES[PALS[pal[y // 16, x // 16]][idx[y, x]]]
Image.fromarray(img, "RGB").save("art.png")
print(f"Saved art.png ({len(tiles)} unique tiles)")
