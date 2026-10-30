import numpy as np
from PIL import Image

W, H = 256, 192
TW, TH = 32, 24

# SMS palette: each RGB channel is one of 0, 85, 170, 255. One 16-colour palette.
PAL = [
    (0, 0, 0),        # 0 black
    (0, 0, 85),       # 1 dark blue (mortar/shadow)
    (85, 85, 85),     # 2 dark gray
    (170, 170, 170),  # 3 light gray
    (85, 85, 170),    # 4 blue-gray stone
    (85, 0, 0),       # 5 dark wood
    (170, 85, 0),     # 6 wood
    (255, 170, 0),    # 7 orange
    (255, 255, 0),    # 8 yellow
    (255, 85, 0),     # 9 red-orange
    (85, 85, 0),      # 10 olive
    (0, 0, 0), (0, 0, 0), (0, 0, 0), (0, 0, 0), (0, 0, 0),
]

rng = np.random.RandomState(7)
canvas = np.zeros((H, W), dtype=np.uint8)


def floor_tile(tx, ty):
    t = np.full((8, 8), 2, dtype=np.uint8)
    if ty % 2 == 0:
        t[0, :] = 1
    if tx % 2 == 0:
        t[:, 0] = 1
    # bevel highlight
    r0 = 1 if ty % 2 == 0 else 0
    c0 = 1 if tx % 2 == 0 else 0
    t[r0, c0:] = np.where(t[r0, c0:] == 2, 4, t[r0, c0:])
    v = rng.randint(0, 3)
    if v == 1:
        t[4, 4] = 1
        t[5, 5] = 1
    elif v == 2:
        t[5, 3] = 3
        t[3, 6] = 1
    return t


def brick_tile():
    t = np.full((8, 8), 4, dtype=np.uint8)
    t[0, :] = 1
    t[4, :] = 1
    t[1:4, 0] = 1
    t[5:8, 4] = 1
    t[1, 1:] = 3
    t[5, :] = np.where(np.arange(8) == 4, 1, 3)
    t[1, 0] = 1
    t[3, 1:] = 2
    t[7, :] = np.where(np.arange(8) == 4, 1, 2)
    v = rng.randint(0, 4)
    if v == 1:
        t[2, 3] = 2
    elif v == 2:
        t[6, 1] = 2
    return t


def cap_tile():
    t = np.full((8, 8), 3, dtype=np.uint8)
    t[7, :] = 1
    t[6, :] = 2
    t[:6, 0] = 2
    return t


# floor
for ty in range(TH):
    for tx in range(TW):
        canvas[ty*8:ty*8+8, tx*8:tx*8+8] = floor_tile(tx, ty)

# walls
for ty in range(TH):
    for tx in range(TW):
        wall = ty <= 4 or ty >= 22 or tx <= 1 or tx >= 30
        if wall:
            t = cap_tile() if ty == 0 else brick_tile()
            canvas[ty*8:ty*8+8, tx*8:tx*8+8] = t

# shadows cast by walls onto floor
canvas[5*8:5*8+3, 16:240] = 1
canvas[40:176, 16:18] = 1
canvas[40:176, 238:240] = 1
canvas[21*8+5:22*8, 16:240] = np.where(canvas[21*8+5:22*8, 16:240] == 2, 2, canvas[21*8+5:22*8, 16:240])

# door: 32x32 at x=112, y=8
dx, dy = 112, 8
d = np.zeros((32, 32), dtype=np.uint8)
d[:, :] = 0
d[0:3, :] = 3
d[:, 0:3] = 3
d[:, 29:32] = 3
d[0:1, :] = 0
d[:, 0] = 0
d[:, 31] = 0
d[0, :] = 0
d[3:3+1, 3:29] = 0
for y in range(4, 32):
    for x in range(3, 29):
        k = (x - 3) % 6
        c = 6
        if k == 0:
            c = 5
        elif k == 1 and y % 5 != 0:
            c = 7
        d[y, x] = c
for by in (10, 24):
    d[by:by+3, 3:29] = 2
    d[by, 3:29] = 3
    for rx in (5, 14, 23):
        d[by+1, rx] = 3
d[17:20, 23:26] = 3
d[18, 24] = 0
d[31, 3:29] = 5
canvas[dy:dy+32, dx:dx+32] = d

# torches: 8x16 at tile cols 9 and 22, rows 2-3
torch = np.full((16, 8), 255, dtype=np.uint8)
bg_mask = np.ones((16, 8), bool)
spr = {}
def put(y, xs, c):
    for x in xs:
        spr[(y, x)] = c
put(0, [3], 9)
put(1, [3, 4], 9)
put(2, [2, 3, 4, 5], 9); put(2, [3, 4], 7)
put(3, [2, 3, 4, 5], 9); put(3, [3, 4], 7)
put(4, [2, 3, 4, 5], 7); put(4, [3, 4], 8)
put(5, [2, 3, 4, 5], 7); put(5, [3, 4], 8)
put(6, range(1, 7), 2)
put(7, range(2, 6), 2)
for y in range(8, 16):
    put(y, [3, 4], 6)
    spr[(y, 3)] = 5
for tx in (9, 22):
    for (y, x), c in spr.items():
        canvas[16 + y, tx*8 + x] = c

# ---- verify hardware limits ----
tiles = {}
for ty in range(TH):
    for tx in range(TW):
        t = canvas[ty*8:ty*8+8, tx*8:tx*8+8]
        cands = [t, t[:, ::-1], t[::-1, :], t[::-1, ::-1]]
        key = min(c.tobytes() for c in cands)
        tiles[key] = 1
print("unique tiles (with flips):", len(tiles))
assert len(tiles) <= 448
assert canvas.max() < 16

rgb = np.array(PAL, dtype=np.uint8)[canvas]
Image.fromarray(rgb, "RGB").save("art.png")
