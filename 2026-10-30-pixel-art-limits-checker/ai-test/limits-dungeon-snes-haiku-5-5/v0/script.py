#!/usr/bin/env python3
"""
Top-down dungeon room background for SNES.
- 256x224 px (full screen), 32x28 tiles of 8x8
- One 4bpp BG palette: 16 colours, index 0 = backdrop
- Colours are 5-bit per channel (BGR555-compatible values)
- Enforces <= 256 unique 8x8 tiles (fits a 4bpp character set)
- Output: art.png (palette-indexed PNG)
"""
import random
from PIL import Image

W, H = 256, 224
TILE = 8
OUT = "art.png"

# 16 colours, 5 bits per channel (SNES native precision)
PAL5 = [
    (1, 1, 2),     # 0  backdrop / void
    (4, 4, 6),     # 1  shadow
    (11, 11, 13),  # 2  stone base
    (17, 17, 19),  # 3  stone light
    (22, 22, 24),  # 4  stone highlight
    (3, 3, 4),     # 5  mortar
    (7, 4, 1),     # 6  wood dark
    (13, 8, 3),    # 7  wood mid
    (19, 12, 5),   # 8  wood light
    (12, 13, 15),  # 9  iron
    (21, 22, 24),  # a  iron highlight
    (31, 9, 2),    # b  flame red
    (31, 18, 3),   # c  flame orange
    (31, 28, 10),  # d  flame yellow
    (6, 6, 7),     # e  dark wall speck
    (14, 10, 4),   # f  torch handle
]
assert len(PAL5) == 16

# Layout (pixel coordinates)
TOP_WALL_H = 24          # rows 0..23 (3 tiles)
BOTTOM_WALL_Y = 208      # rows 208..223 (2 tiles)
SIDE_WALL_W = 16         # columns 0..15 and 240..255 (2 tiles each)
DOOR_X, DOOR_Y, DOOR_W, DOOR_H = 112, 0, 32, 24
TORCH_XS = (72, 176)     # mirrored about the door centre

FLAME = [
    "...bb...",
    "..bccb..",
    ".bcddcb.",
    ".bcddcb.",
    "..bccb..",
    "...bb...",
    "........",
    "........",
]
HANDLE = [
    "..9999..",
    "..9aa9..",
    "...ff...",
    "...ff...",
    "...ff...",
    "...ff...",
    "..9999..",
    "........",
]


def make_floor_tile(rnd):
    """16x16 flagstone, tiled across the floor (period 16 px)."""
    t = [[2] * 16 for _ in range(16)]
    for y in range(16):
        for x in range(16):
            if x == 0 or y == 0:
                t[y][x] = 5                       # mortar joints
            elif x == 1 or y == 1:
                t[y][x] = 4                       # bevel highlight
            else:
                r = rnd.random()
                if r < 0.08:
                    t[y][x] = 3
                elif r < 0.13:
                    t[y][x] = 1
    return t


def wall_index(x, y):
    """Running-bond stone blocks, period 16 px in both directions."""
    row = y // 8
    by = y % 8
    bx = (x + (8 if row % 2 else 0)) % 16
    if by == 7 or bx == 0:
        return 5                                  # mortar
    if by == 0:
        return 4                                  # top bevel
    if by == 6:
        return 1                                  # bottom shadow
    h = (bx * 5 + by * 3) % 7
    if h == 0:
        return 3
    if h == 4 and (bx + by) % 3 == 0:
        return 14                                 # dark speck
    return 2


def door_index(lx, ly):
    """Wooden door, local coords (0..DOOR_W-1, 0..DOOR_H-1)."""
    if lx in (0, DOOR_W - 1) or ly in (0, DOOR_H - 1):
        return 6                                  # frame / edges
    if ly in (6, 17):                             # iron bands
        if lx in (3, DOOR_W - 4):
            return 10                             # rivets
        return 9
    if lx % 8 == 0:
        return 6                                  # plank seams
    return 8 if (lx * 7 + ly * 3) % 11 < 3 else 7  # grain


def stamp(idx, sprite, x0, y0):
    """Draw a sprite; '.' is transparent (keeps what is underneath)."""
    for j, row in enumerate(sprite):
        for i, c in enumerate(row):
            if c != ".":
                idx[y0 + j][x0 + i] = int(c, 16)


def build_palette_bytes():
    out = []
    for r, g, b in PAL5:
        # Expand 5-bit to 8-bit by bit replication (exact round-trip)
        out += [(r << 3) | (r >> 2), (g << 3) | (g >> 2), (b << 3) | (b >> 2)]
    out += [0] * (768 - len(out))
    return bytes(out)


def main():
    rnd = random.Random(7)
    floor_tile = make_floor_tile(rnd)

    idx = [[0] * W for _ in range(H)]

    # Walls and floor
    for y in range(H):
        for x in range(W):
            in_floor = (SIDE_WALL_W <= x < W - SIDE_WALL_W and
                        TOP_WALL_H <= y < BOTTOM_WALL_Y)
            if in_floor:
                idx[y][x] = floor_tile[(y - TOP_WALL_H) % 16][(x - SIDE_WALL_W) % 16]
            else:
                idx[y][x] = wall_index(x, y)

    # Door set into the top wall
    for ly in range(DOOR_H):
        for lx in range(DOOR_W):
            idx[DOOR_Y + ly][DOOR_X + lx] = door_index(lx, ly)

    # Torches on the top wall, flanking the door
    for tx in TORCH_XS:
        stamp(idx, FLAME, tx, 8)
        stamp(idx, HANDLE, tx, 16)

    # Validate
    max_index = max(max(row) for row in idx)
    assert max_index < 16, "palette index out of range for one 4bpp palette"

    tiles = {}
    for ty in range(H // TILE):
        for tx in range(W // TILE):
            key = tuple(
                idx[ty * TILE + j][tx * TILE + i]
                for j in range(TILE) for i in range(TILE)
            )
            tiles.setdefault(key, len(tiles))
    assert len(tiles) <= 256, f"too many unique tiles: {len(tiles)}"

    flat = bytes(v for row in idx for v in row)
    img = Image.frombytes("P", (W, H), flat)
    img.putpalette(build_palette_bytes())
    img.save(OUT)

    print(f"Saved {OUT}: {W}x{H}, {len(PAL5)} colours, "
          f"{len(tiles)} unique 8x8 tiles")


if __name__ == "__main__":
    main()
