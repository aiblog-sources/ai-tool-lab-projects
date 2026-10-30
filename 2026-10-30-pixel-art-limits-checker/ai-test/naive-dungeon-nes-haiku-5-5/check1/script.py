import math
from PIL import Image

W, H = 256, 240

# Shared backdrop colour (also used for mortar, shadows and outlines)
BG = (40, 36, 46)

# 4 palettes x 3 colours = 12 colours, plus the shared backdrop
WALL_A, WALL_B, WALL_HI = (72, 64, 76), (92, 84, 96), (124, 116, 128)
FLOOR_A, FLOOR_B, FLOOR_HI = (88, 88, 98), (104, 102, 112), (128, 126, 136)
WOOD, WOOD_L, WOOD_D = (136, 84, 40), (168, 112, 64), (80, 48, 24)
IRON, FLAME_O, FLAME_Y = (160, 162, 170), (248, 120, 24), (252, 200, 48)

TORCHES = [(32, 48), (208, 48)]          # top-left corners of the 16x16 torch cells
TORCH_CENTRES = [(40, 56), (216, 56)]    # centres used for the floor light
SPECKLES = {                             # fixed speckles, same in every flagstone
    (3, 5): FLOOR_HI, (10, 2): BG, (6, 11): FLOOR_HI,
    (12, 9): BG, (2, 13): FLOOR_HI,
}


def wall_px(x, y):
    """Staggered 16x8 stone blocks. Palette: WALL_A, WALL_B, WALL_HI + backdrop."""
    row = y // 8
    bx = x + (8 if row % 2 else 0)       # alternate rows are offset by half a block
    lx, ly = bx % 16, y % 8
    if ly == 7 or lx == 15:
        return BG                        # mortar
    if ly == 0:
        return WALL_HI                   # top highlight
    return WALL_A if (bx // 16 + row) % 2 == 0 else WALL_B


def floor_px(x, y):
    """16x16 flagstones. Palette: FLOOR_A/FLOOR_B, FLOOR_HI + backdrop."""
    if x in (16, 17, 238, 239) or y in (16, 17, 222, 223):
        return BG                        # inner-wall shadow
    lx, ly = (x - 16) % 16, (y - 16) % 16
    if lx == 15 or ly == 15:
        return BG                        # mortar
    if (lx, ly) in SPECKLES:
        return SPECKLES[(lx, ly)]
    if lx == 0 or ly == 0:
        return FLOOR_HI                  # highlight edges
    cx0, cy0 = x - lx, y - ly            # top-left of this flagstone
    lit = any((cx0 + 8 - tx) ** 2 + (cy0 + 8 - ty) ** 2 < 56 ** 2
              for tx, ty in TORCH_CENTRES)
    return FLOOR_B if lit else FLOOR_A


def door_px(x, y):
    """Wooden door filling one 16x16 cell. Palette: WOOD, WOOD_L, WOOD_D."""
    lx = x - 128
    if lx in (0, 15) or y == 15:
        return WOOD_D                    # frame and threshold
    if lx in (3, 7, 11) or y in (4, 11):
        return WOOD_D                    # plank seams and iron-band stand-ins
    if lx in (8, 9) and y in (7, 8):
        return WOOD_D                    # handle
    return WOOD if (lx // 4) % 2 == 0 else WOOD_L


def torch_px(x, y, x0, y0):
    """Top-down torch in a 16x16 cell. Palette: IRON, FLAME_O, FLAME_Y + backdrop."""
    px, py = x - x0 + 0.5, y - y0 + 0.5  # pixel centres
    d_flame = math.hypot(px - 8, py - 5)
    d_brazier = math.hypot(px - 8, py - 11)
    if d_flame <= 1.8:
        return FLAME_Y                   # hot core
    if d_flame <= 3.6:
        return FLAME_O                   # outer flame
    if d_brazier <= 5.0:
        return IRON                      # brazier bowl
    return BG


def pixel(x, y):
    for tx, ty in TORCHES:
        if tx <= x < tx + 16 and ty <= y < ty + 16:
            return torch_px(x, y, tx, ty)
    if y < 16 and 128 <= x < 144:
        return door_px(x, y)
    if 16 <= x < 240 and 16 <= y < 224:
        return floor_px(x, y)
    return wall_px(x, y)


img = Image.new("RGB", (W, H))
img.putdata([pixel(x, y) for y in range(H) for x in range(W)])
img.save("art.png")
