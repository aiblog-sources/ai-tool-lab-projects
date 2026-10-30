import numpy as np
from PIL import Image

# Output: 320x224 (full Mega Drive screen = 40x28 tiles of 8x8)
W, H = 320, 224
OUT = "art.png"

# Mega Drive colors: 3 bits per channel (levels 0..7), 9-bit RGB.
# One 16-color palette line, index 0 = backdrop/black.
LEVELS = [
    (0, 0, 0),  # 0  black / outline
    (1, 1, 1),  # 1  mortar
    (3, 3, 3),  # 2  floor dark
    (4, 4, 4),  # 3  floor mid
    (5, 5, 5),  # 4  floor light
    (2, 2, 3),  # 5  wall shadow mortar
    (3, 3, 4),  # 6  wall mid
    (5, 5, 6),  # 7  wall light
    (3, 1, 0),  # 8  wood dark
    (5, 3, 1),  # 9  wood mid
    (6, 4, 2),  # 10 wood light
    (4, 4, 5),  # 11 iron
    (7, 2, 0),  # 12 flame red
    (7, 6, 1),  # 13 flame yellow
    (4, 2, 0),  # 14 torch handle
    (7, 7, 7),  # 15 highlight white
]
K, MORT, FD, FM, FL = 0, 1, 2, 3, 4
WD, WM, WL = 5, 6, 7
OD, OM, OL = 8, 9, 10
IRON, RED, YEL, HAND, WHITE = 11, 12, 13, 14, 15

assert len(LEVELS) <= 16
for r, g, b in LEVELS:
    assert all(0 <= c <= 7 for c in (r, g, b))

PAL8 = [tuple(round(c * 255 / 7) for c in lv) for lv in LEVELS]

rng = np.random.default_rng(7)
img = np.zeros((H, W), dtype=np.uint8)
INTERIOR = (24, 208, 16, 304)  # y0, y1, x0, x1 of the floor area


def put(y0, y1, x0, x1, v, box=(0, H, 0, W)):
    by0, by1, bx0, bx1 = box
    y0, y1 = max(y0, by0), min(y1, by1)
    x0, x1 = max(x0, bx0), min(x1, bx1)
    if y0 < y1 and x0 < x1:
        img[y0:y1, x0:x1] = v


# ---- Walls: staggered brick pattern over the whole screen ----
img[:] = WM
for r, y in enumerate(range(0, H, 8)):
    off = 8 if r % 2 else 0
    for x in range(-off, W, 16):
        put(y, y + 8, x, x + 16, WD)                      # mortar
        v = WL if rng.random() < 0.35 else WM
        put(y + 1, y + 8, x + 1, x + 16, v)               # brick face
        put(y + 1, y + 2, x + 1, x + 16, WL)              # top highlight

# ---- Floor: flagstones inside the walls ----
y0, y1, x0, x1 = INTERIOR
put(y0, y1, x0, x1, FD, INTERIOR)
for r, y in enumerate(range(y0, y1, 16)):
    off = 8 if r % 2 else 0
    for x in range(x0 - off, x1, 16):
        c = rng.choice([FD, FM, FL], p=[0.25, 0.5, 0.25])
        put(y, y + 16, x, x + 16, MORT, INTERIOR)         # joint outline
        put(y + 1, y + 16, x + 1, x + 16, c, INTERIOR)    # stone body
        if c != FD:
            put(y + 14, y + 16, x + 1, x + 16, FD, INTERIOR)  # bottom shade
            put(y + 1, y + 16, x + 14, x + 16, FD, INTERIOR)  # right shade
        if c == FL:
            put(y + 1, y + 2, x + 1, x + 14, WHITE, INTERIOR)  # highlight

# Dark outline where the floor meets the walls
put(y0, y0 + 1, x0, x1, K)
put(y1 - 1, y1, x0, x1, K)
put(y0, y1, x0, x0 + 1, K)
put(y0, y1, x1 - 1, x1, K)

# ---- Wooden door in the top wall, centered ----
DX0, DX1 = 144, 176
put(0, 24, DX0 - 2, DX1 + 2, K)                           # frame/outline
put(0, 24, DX0, DX1, OM)
for x, col in [(DX0, OL), (DX0 + 8, OM), (DX0 + 16, OL), (DX0 + 24, OM)]:
    put(0, 24, x, x + 8, col)
for x in (DX0 + 8, DX0 + 16, DX0 + 24):
    put(0, 24, x, x + 1, OD)                              # plank seams
put(6, 8, DX0, DX1, IRON)                                 # iron bands
put(17, 19, DX0, DX1, IRON)
put(11, 14, DX1 - 8, DX1 - 5, WHITE)                      # door knob

# ---- Two torches flanking the door ----
def torch(cx):
    put(16, 22, cx - 1, cx + 1, HAND)                     # handle
    put(13, 17, cx - 2, cx + 2, IRON)                     # cup
    for y in range(4, 14):
        for x in range(cx - 5, cx + 6):
            if not (0 <= y < H and 0 <= x < W):
                continue
            dx, dy = x - cx, y - 11
            d = (dx * dx + dy * dy) ** 0.5
            if d <= 1.5:
                img[y, x] = WHITE
            elif d <= 2.7:
                img[y, x] = YEL
            elif d <= 4.0 and y <= 12:
                img[y, x] = RED


torch(90)
torch(230)

# ---- Build a Mega Drive-style indexed image ----
flat_pal = [c for rgb in PAL8 for c in rgb]
flat_pal += [0] * (768 - len(flat_pal))
out = Image.new("P", (W, H))
out.frombytes(img.tobytes())
out.putpalette(flat_pal)

# ---- Hardware checks ----
used = set(np.unique(img).tolist())
assert used <= set(range(16)), "more than 16 colors in the palette line"
tiles = img.reshape(H // 8, 8, W // 8, 8).transpose(0, 2, 1, 3).reshape(-1, 64)
unique_tiles = len({t.tobytes() for t in tiles})
assert unique_tiles <= 2048, "too many unique tiles for VRAM"
assert len(tiles) == 1120  # 40x28 cells in the plane

out.save(OUT)
print(f"Saved {OUT}: {W}x{H}, {len(used)} colors in one 16-color line, "
      f"{unique_tiles} unique 8x8 tiles")
