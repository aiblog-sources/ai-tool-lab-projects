import numpy as np
from PIL import Image

# Full Master System screen, drawn on the 8x8 tile grid
W, H = 256, 192
TW, TH = 32, 24
OUT = "art.png"

# SMS color RAM: 2 bits per channel, so each channel is 0, 85, 170 or 255.
LEVELS = (0, 85, 170, 255)

# One background palette of 16 colors. Index 0 is the backdrop color.
PAL = [
    (0, 0, 0),        # 0  backdrop / outline
    (0, 0, 85),       # 1  wall shadow
    (85, 85, 170),    # 2  wall stone
    (170, 170, 255),  # 3  wall highlight
    (85, 85, 85),     # 4  iron / mortar
    (170, 170, 170),  # 5  floor stone
    (255, 255, 255),  # 6  floor highlight
    (170, 85, 0),     # 7  wood
    (255, 170, 85),   # 8  wood highlight
    (85, 0, 0),       # 9  dark wood / frame
    (255, 85, 0),     # 10 flame
    (255, 255, 0),    # 11 flame core
    (255, 170, 0),    # 12 gold (door knob)
] + [(0, 0, 0)] * 3   # 13-15 unused
assert len(PAL) == 16
assert all(v in LEVELS for col in PAL for v in col)

BLACK, SHADOW, WALL, WALL_HI, IRON, FLOOR, FLOOR_HI = 0, 1, 2, 3, 4, 5, 6
WOOD, WOOD_LT, WOOD_DK, FLAME, FLAME_CORE, GOLD = 7, 8, 9, 10, 11, 12


def new_tile(fill=0):
    return np.full((8, 8), fill, dtype=np.uint8)


# Floor: stone blocks with mortar on the top and/or left edge, plus speckle variants
SPECKLES = [
    [],
    [(3, 4, FLOOR_HI)],
    [(5, 2, IRON), (2, 6, FLOOR_HI)],
    [(6, 5, FLOOR_HI), (1, 3, IRON)],
]


def floor_tile(top, left, spk):
    t = new_tile(FLOOR)
    if top:
        t[0, :] = IRON
    if left:
        t[:, 0] = IRON
    for y, x, col in SPECKLES[spk]:
        t[y, x] = col
    return t


# Walls: staggered brick face and a bright cap on the outer edge
def wall_face(parity):
    t = new_tile(WALL)
    t[3, :] = SHADOW
    t[7, :] = SHADOW
    t[0:3, 0 if parity == 0 else 4] = SHADOW
    t[4:7, 4 if parity == 0 else 0] = SHADOW
    return t


def wall_cap():
    t = new_tile(WALL)
    t[0, :] = WALL_HI
    t[7, :] = SHADOW
    return t


def stamp(t, art, legend):
    for y, row in enumerate(art):
        for x, ch in enumerate(row):
            if ch in legend:
                t[y, x] = legend[ch]
    return t


# Torch flame and bracket, drawn over a wall tile ('.' keeps the wall pixel)
FLAME_ART = [
    "........",
    "...o....",
    "..ooo...",
    ".ooyoo..",
    ".oyyyo..",
    "..yyy...",
    "...y....",
    "........",
]
FLAME_LEG = {"o": FLAME, "y": FLAME_CORE}

BRACKET_ART = [
    "...77...",
    "...77...",
    "...77...",
    "..4444..",
    "..4..4..",
    "........",
    "........",
    "........",
]
BRACKET_LEG = {"7": WOOD, "4": IRON}


# Wooden door, 16x24 pixels = 2x3 tiles
def door_image():
    d = np.full((24, 16), WOOD, dtype=np.uint8)
    d[:, 0::4] = WOOD_DK      # plank seams
    d[:, 1::4] = WOOD_LT      # plank highlights
    d[0, :] = WOOD_DK         # frame
    d[-1, :] = WOOD_DK
    d[:, 0] = WOOD_DK
    d[:, -1] = WOOD_DK
    d[6, :] = IRON            # iron bands
    d[17, :] = IRON
    d[11:13, 11:13] = GOLD    # knob
    return d


# Compose the 256x192 index map on the tile grid
A = np.zeros((H, W), dtype=np.uint8)


def put(r, c, t):
    A[r * 8:r * 8 + 8, c * 8:c * 8 + 8] = t


for r in range(TH):
    for c in range(TW):
        outer = r in (0, TH - 1) or c in (0, TW - 1)
        in_wall = outer or r < 3 or r > 20 or c < 2 or c > 29
        if outer:
            t = wall_cap()
        elif in_wall:
            t = wall_face((r + c) % 2)
        else:
            top = (r % 2 == 0)
            left = ((c + r // 2) % 2 == 0)
            spk = (r * 5 + c * 3 + r * c) % 4
            t = floor_tile(top, left, spk)
        put(r, c, t)

# Two torches on the top wall, mirrored around the door
for col in (6, 25):
    put(1, col, stamp(wall_face((1 + col) % 2), FLAME_ART, FLAME_LEG))
    put(2, col, stamp(wall_face((2 + col) % 2), BRACKET_ART, BRACKET_LEG))

# Door centered in the top wall: columns 15-16, rows 0-2
D = door_image()
for dr in range(3):
    for dc in range(2):
        put(dr, 15 + dc, D[dr * 8:dr * 8 + 8, dc * 8:dc * 8 + 8])

# Hardware checks
assert A.max() < 16, "pixel index outside the 16-color background palette"

tile_patterns = set()
for r in range(TH):
    for c in range(TW):
        tile_patterns.add(A[r * 8:r * 8 + 8, c * 8:c * 8 + 8].tobytes())
assert len(tile_patterns) <= 256, f"too many unique tiles: {len(tile_patterns)}"

print(f"Unique 8x8 tiles: {len(tile_patterns)} (SMS limit per bank: 256)")
print(f"Colors used: {len(np.unique(A))} of 16 background palette entries")

# Save as an indexed PNG so the 16-entry palette is preserved exactly
img = Image.new("P", (W, H))
img.putdata(A.ravel().tolist())
img.putpalette([v for col in PAL for v in col])
img.save(OUT)
print(f"Saved {OUT}")
