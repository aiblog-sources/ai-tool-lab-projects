import numpy as np
from PIL import Image, ImageDraw

W, H = 256, 240
rng = np.random.default_rng(7)  # fixed seed so output is deterministic

# ---------------------------------------------------------------------------
# NES constraints honoured here:
#  * one shared backdrop colour (UNIV) for mortar, shadows and the door frame
#  * background: 16x16 attribute cells, each cell uses ONE palette of
#    3 colours; every 8x8 tile therefore has <= 3 colours + backdrop
#  * torches are 8x8 sprites from 2 sprite palettes (index 0 = transparent),
#    at most 8 sprites per scanline
#  * <= 25 colours on screen in total
# The old continuous warm-light blend is replaced by a "lit" floor palette.
# ---------------------------------------------------------------------------

UNIV = (34, 30, 40)  # shared backdrop colour

WALL_A, WALL_B, WALL_HI = (72, 64, 76), (92, 84, 96), (124, 116, 128)
FLOOR_A, FLOOR_B, FLOOR_HI = (88, 88, 98), (104, 102, 112), (128, 126, 136)
LIT_A, LIT_B, LIT_HI = (138, 113, 93), (149, 122, 102), (166, 139, 119)
WOOD, WOOD_L, IRON = (136, 84, 40), (168, 112, 64), (160, 162, 170)
BRAZIER, RIM, COALS = (72, 48, 32), (128, 76, 36), (200, 80, 24)
FLAME_O, FLAME_Y, FLAME_W = (248, 120, 24), (252, 200, 48), (255, 248, 176)

BG_PALETTES = {
    "wall": (WALL_A, WALL_B, WALL_HI),
    "floor": (FLOOR_A, FLOOR_B, FLOOR_HI),
    "lit": (LIT_A, LIT_B, LIT_HI),
    "wood": (WOOD, WOOD_L, IRON),
}
SPRITE_PALETTES = {
    "brazier": (BRAZIER, RIM, COALS),
    "flame": (FLAME_O, FLAME_Y, FLAME_W),
}

TORCHES = [(44, 52), (212, 52)]  # torch centres
LIGHT_RADIUS = 46

img = Image.new("RGB", (W, H), UNIV)
d = ImageDraw.Draw(img)

# Every 16x16 attribute cell gets exactly one background palette.
cell_pal = {}
for y0 in range(0, H, 16):
    for x0 in range(0, W, 16):
        cell_pal[(x0, y0)] = "wall"


def brick_wall(x0, y0, x1, y1):
    """Staggered 16x8 stone blocks with mortar lines."""
    for by in range(y0, y1, 8):
        row = (by - y0) // 8
        off = 8 if row % 2 else 0
        for bx in range(x0 - off, x1, 16):
            c = WALL_A if rng.random() < 0.5 else WALL_B
            left, right = max(bx, x0), min(bx + 15, x1 - 1)
            if left > right:
                continue
            d.rectangle([left, by, right, by + 7], fill=c)
            d.line([(left, by + 7), (right, by + 7)], fill=UNIV)      # bottom mortar
            if bx + 15 <= x1 - 1 and bx >= x0:
                d.line([(bx + 15, by), (bx + 15, by + 7)], fill=UNIV)  # right mortar
            d.line([(left, by), (right, by)], fill=WALL_HI)           # top highlight


brick_wall(0, 0, W, H)


def near_torch(x, y):
    return any((x - tx) ** 2 + (y - ty) ** 2 < LIGHT_RADIUS ** 2 for tx, ty in TORCHES)


# Flagstone floor inside the 2-tile wall border: x 16..239, y 16..223
for gy in range(16, 224, 16):
    for gx in range(16, 240, 16):
        pal = "lit" if near_torch(gx + 8, gy + 8) else "floor"
        cell_pal[(gx, gy)] = pal
        a, b, hi = BG_PALETTES[pal]
        c = a if rng.random() < 0.5 else b
        d.rectangle([gx, gy, gx + 15, gy + 15], fill=c)
        d.line([(gx + 15, gy), (gx + 15, gy + 15)], fill=UNIV)
        d.line([(gx, gy + 15), (gx + 15, gy + 15)], fill=UNIV)
        d.line([(gx, gy), (gx + 14, gy)], fill=hi)
        d.line([(gx, gy), (gx, gy + 14)], fill=hi)
        for _ in range(3):  # speckles
            px = gx + int(rng.integers(1, 14))
            py = gy + int(rng.integers(1, 14))
            d.point((px, py), fill=hi if rng.random() < 0.5 else UNIV)

# Wall-to-floor shadow along the inner edges
d.rectangle([16, 16, 239, 17], fill=UNIV)
d.rectangle([16, 222, 239, 223], fill=UNIV)
d.rectangle([16, 16, 17, 223], fill=UNIV)
d.rectangle([238, 16, 239, 223], fill=UNIV)

# Cast shadows of the torches (background layer, backdrop colour)
for cx, cy in TORCHES:
    d.ellipse([cx - 6, cy - 4, cx + 7, cy + 7], fill=UNIV)

# Wooden door on the attribute grid: x 112..127, y 0..15
DX, DY = 112, 0
cell_pal[(DX, DY)] = "wood"
d.rectangle([DX, DY, DX + 15, DY + 15], fill=UNIV)            # dark frame
for i, px in enumerate(range(DX + 1, DX + 15, 4)):            # 3px planks, 1px seams
    c = WOOD if i % 2 == 0 else WOOD_L
    d.rectangle([px, DY + 1, min(px + 2, DX + 14), DY + 14], fill=c)
d.rectangle([DX + 1, 4, DX + 14, 4], fill=IRON)               # iron bands
d.rectangle([DX + 1, 11, DX + 14, 11], fill=IRON)
d.rectangle([DX + 12, 7, DX + 13, 8], fill=IRON)              # handle
d.rectangle([DX, 15, DX + 15, 15], fill=UNIV)                 # threshold


# --- background check: every 8x8 tile must fit its cell's palette -----------
def check_background(im):
    arr = np.array(im)
    for (x0, y0), name in cell_pal.items():
        allowed = set(BG_PALETTES[name]) | {UNIV}
        for ty in range(y0, y0 + 16, 8):
            for tx in range(x0, x0 + 16, 8):
                tile = arr[ty:ty + 8, tx:tx + 8].reshape(-1, 3)
                cols = {tuple(int(v) for v in c) for c in tile}
                assert cols <= allowed, f"tile {(tx, ty)} breaks palette '{name}'"
                assert len(cols - {UNIV}) <= 3


check_background(img)

# --- torch sprites -----------------------------------------------------------
# 16x16 art: top 8 rows = flame (sprite palette "flame"),
#            bottom 8 rows = brazier (sprite palette "brazier").
torch = Image.new("RGBA", (16, 16), (0, 0, 0, 0))
td = ImageDraw.Draw(torch)


def opaque(c):
    return c + (255,)


td.ellipse([3, 8, 12, 14], fill=opaque(BRAZIER))
td.ellipse([4, 9, 11, 13], fill=opaque(RIM))
td.ellipse([5, 10, 10, 12], fill=opaque(COALS))
td.ellipse([4, 0, 11, 7], fill=opaque(FLAME_O))
td.ellipse([6, 2, 9, 6], fill=opaque(FLAME_Y))
td.point((7, 4), fill=opaque(FLAME_W))

# Four 8x8 sprites per torch; check each one against its sprite palette.
SPRITE_TILES = []  # (dx, dy, palette) relative to the torch's top-left
t_arr = np.array(torch)
for dy in (0, 8):
    for dx in (0, 8):
        pal = "flame" if dy == 0 else "brazier"
        tile = t_arr[dy:dy + 8, dx:dx + 8].reshape(-1, 4)
        cols = {tuple(int(v) for v in c[:3]) for c in tile if c[3] == 255}
        assert cols <= set(SPRITE_PALETTES[pal]) and len(cols) <= 3
        SPRITE_TILES.append((dx, dy, pal))

# Sprites per scanline must stay within the NES limit of 8.
per_line = [0] * H
for cx, cy in TORCHES:
    for dx, dy, _ in SPRITE_TILES:
        for y in range(cy - 8 + dy, cy - 8 + dy + 8):
            per_line[y] += 1
assert max(per_line) <= 8

for cx, cy in TORCHES:
    ox, oy = cx - 8, cy - 8
    img.paste(torch.convert("RGB"), (ox, oy), torch.split()[3])

# --- global colour budget: at most 25 colours on screen ----------------------
all_cols = {tuple(int(v) for v in c) for c in np.array(img).reshape(-1, 3)}
assert len(all_cols) <= 25, len(all_cols)

img.save("art.png")
