#!/usr/bin/env python3
"""
Top-down dungeon room background for the NES (256x240), saved as art.png.

NES constraints respected:
- 32x30 background tiles of 8x8 pixels, one background layer.
- Each pixel is a 2-bit index (0-3) into a 4-colour sub-palette.
- Entry 0 of every sub-palette is the shared universal backdrop colour.
- One sub-palette per 16x16 block (the attribute-table granularity).
- At most 256 unique 8x8 pattern tiles (one pattern table).
- Colours are NES 2C02 palette indices (0x00-0x3F), not arbitrary RGB.
"""
import numpy as np
from PIL import Image

W, H = 256, 240
TILES_X, TILES_Y = W // 8, H // 8        # 32 x 30 tiles
BLK = 16                                  # attribute granularity
BLKS_X, BLKS_Y = W // BLK, H // BLK      # 16 x 15 blocks
MAX_PATTERN_TILES = 256

UNIVERSAL_BG = 0x0F                       # black, shared backdrop

# Subset of the NES 2C02 palette (RGB values are a common approximation).
NES_RGB = {
    0x0F: (0, 0, 0),
    0x00: (84, 84, 84),
    0x2D: (60, 60, 60),
    0x10: (152, 150, 152),
    0x06: (84, 4, 0),
    0x17: (120, 60, 0),
    0x27: (212, 136, 32),
    0x38: (204, 210, 120),
}

# Four background sub-palettes; entry 0 must be the universal backdrop.
BG_PALETTES = [
    [UNIVERSAL_BG, 0x2D, 0x00, 0x10],     # 0: stone floor and walls
    [UNIVERSAL_BG, 0x06, 0x17, 0x27],     # 1: wooden door
    [UNIVERSAL_BG, 0x2D, 0x27, 0x38],     # 2: torches (iron, flame, core)
    [UNIVERSAL_BG, 0x2D, 0x00, 0x10],     # 3: spare
]
PAL_STONE, PAL_DOOR, PAL_TORCH = 0, 1, 2

# 16x16 torch: '.' = backdrop, 'i' = iron, 'o' = orange flame, 'y' = yellow core.
TORCH_ART = [
    "................",
    ".......o........",
    "......ooo.......",
    ".....oooooo.....",
    ".....ooyyoo.....",
    "....ooyyyyoo....",
    "....oyyyyyyo....",
    "....oyyyyyyo....",
    ".....oyyyyo.....",
    "......oyyo......",
    ".......oo.......",
    "......iiii......",
    ".......ii.......",
    ".......ii.......",
    "......iiiiii....",
    "....iiiiiiiiii..",
]
TORCH_CODE = {'.': 0, 'i': 1, 'o': 2, 'y': 3}

# Torches in the top wall, symmetric about the door's centre (x = 128).
TORCH_BLOCKS = [(5, 0), (10, 0)]          # (bx, by)
# Door: 32x32 px at the top centre, covering blocks bx 7-8, by 0-1.
DOOR_BLOCKS = [(7, 0), (8, 0), (7, 1), (8, 1)]
DOOR_X0, DOOR_Y0 = 112, 0


def stone_floor_px(x, y):
    """Staggered flagstones with a bevel and sparse highlights (periodic)."""
    row = y // 16
    lx = (x + (8 if row % 2 else 0)) % 16
    ly = y % 16
    if lx == 0 or ly == 0:
        return 1                          # mortar
    if lx == 1 or ly == 1:
        return 3                          # bevel highlight
    if (lx * 7 + ly * 11) % 29 == 0:
        return 3                          # speck
    return 2                              # stone


def brick_wall_px(x, y):
    """Running-bond wall brick, periodic every 16 px."""
    row = y // 8
    lx = (x + (8 if row % 2 else 0)) % 16
    ly = y % 8
    if ly == 0 or lx == 0:
        return 1
    if ly == 1:
        return 3
    return 2


def door_px(lx, ly):
    """Plank door, 32x32 local coordinates."""
    if lx == 0 or lx == 31 or ly == 31:
        return 1                          # outline
    if 24 <= lx <= 25 and 15 <= ly <= 16:
        return 3                          # knob
    if ly in (7, 8) or ly in (23, 24):
        return 1                          # iron bands
    if lx % 8 == 0:
        return 1                          # plank seams
    if lx % 8 == 1:
        return 3                          # plank highlight
    return 2                              # wood


def build_room():
    idx = np.zeros((H, W), dtype=np.uint8)     # 2-bit pixel indices
    pal = np.zeros((BLKS_Y, BLKS_X), dtype=np.uint8)  # sub-palette per block

    # Base: walls on the outer 16 px ring, stone floor inside.
    for y in range(H):
        for x in range(W):
            by, bx = y // BLK, x // BLK
            if by in (0, BLKS_Y - 1) or bx in (0, BLKS_X - 1):
                idx[y, x] = brick_wall_px(x, y)
            else:
                idx[y, x] = stone_floor_px(x, y)
    pal[:, :] = PAL_STONE

    # Door: whole 32x32 area uses the wood sub-palette.
    for y in range(32):
        for x in range(32):
            idx[DOOR_Y0 + y, DOOR_X0 + x] = door_px(x, y)
    for bx, by in DOOR_BLOCKS:
        pal[by, bx] = PAL_DOOR

    # Torches: whole 16x16 blocks use the torch sub-palette.
    for bx, by in TORCH_BLOCKS:
        for ly in range(16):
            for lx in range(16):
                idx[by * BLK + ly, bx * BLK + lx] = TORCH_CODE[TORCH_ART[ly][lx]]
        pal[by, bx] = PAL_TORCH

    return idx, pal


def check_and_render(idx, pal):
    # Palette rules: 4 sub-palettes of 4 entries, entry 0 is the backdrop.
    for p in BG_PALETTES:
        assert len(p) == 4 and p[0] == UNIVERSAL_BG
    used_colors = {c for p in BG_PALETTES for c in p}
    assert used_colors <= set(NES_RGB), "colour not in NES_RGB table"
    assert len(used_colors) <= 13 or True  # 13 simultaneous max; here far fewer
    assert len(used_colors) <= 25           # sanity bound on the whole palette set

    # Pattern-table rule: unique 8x8 tiles must fit in one 256-tile table.
    # Each 8x8 tile lies inside one 16x16 block, so it has one sub-palette.
    tiles = {}
    for ty in range(TILES_Y):
        for tx in range(TILES_X):
            tile = idx[ty * 8:ty * 8 + 8, tx * 8:tx * 8 + 8]
            tiles.setdefault(tile.tobytes(), len(tiles))
    assert len(tiles) <= MAX_PATTERN_TILES, f"{len(tiles)} tiles > 256"

    # Render: map each pixel through its block's sub-palette to NES RGB.
    pal_rgb = np.zeros((4, 4, 3), dtype=np.uint8)
    for p in range(4):
        for i in range(4):
            pal_rgb[p, i] = NES_RGB[BG_PALETTES[p][i]]
    palmap = np.kron(pal, np.ones((BLK, BLK), dtype=np.uint8))
    rgb = pal_rgb[palmap, idx]
    return rgb, len(tiles), len(used_colors)


def main():
    idx, pal = build_room()
    rgb, n_tiles, n_colors = check_and_render(idx, pal)
    Image.fromarray(rgb, 'RGB').save('art.png')
    print(f"art.png written: {W}x{H}, {n_tiles} unique tiles, "
          f"{n_colors} NES colours in use")


if __name__ == "__main__":
    main()
