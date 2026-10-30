import random
import numpy as np
from PIL import Image

# Approximate NES (2C02) colors
NES = {
    0x0F: (0, 0, 0), 0x00: (124, 124, 124), 0x10: (188, 188, 188),
    0x01: (0, 0, 188), 0x08: (80, 48, 0), 0x18: (172, 124, 0),
    0x28: (248, 184, 0), 0x06: (168, 16, 0), 0x16: (228, 92, 16),
}
BACKDROP = 0x0F
PALS = [
    [BACKDROP, 0x00, 0x10, 0x01],  # 0 floor
    [BACKDROP, 0x01, 0x00, 0x10],  # 1 walls
    [BACKDROP, 0x08, 0x18, 0x28],  # 2 door
    [BACKDROP, 0x06, 0x16, 0x28],  # 3 torch
]

W, H = 256, 240
idx = np.zeros((H, W), dtype=np.uint8)
attr = np.zeros((H // 16, W // 16), dtype=np.uint8)  # palette per 16x16 block


def floor_block(seed):
    r = random.Random(seed)
    b = np.full((16, 16), 1, np.uint8)
    b[15, :] = 0
    b[:, 15] = 0
    b[0, 0:14] = 2
    b[0:14, 0] = 2
    b[8, 1:14] = 1
    for _ in range(r.randint(2, 5)):
        b[r.randint(2, 13), r.randint(2, 13)] = r.choice([2, 3])
    if seed % 3 == 0:  # crack
        x, y = r.randint(4, 10), r.randint(3, 6)
        for i in range(5):
            b[y + i, min(13, x + (i % 2))] = 0
    return b


def wall_block(seed):
    r = random.Random(seed)
    b = np.full((16, 16), 2, np.uint8)
    for row in range(2):
        y0 = row * 8
        off = 0 if row == 0 else 8
        b[y0 + 0, :] = 3
        b[y0 + 6, :] = 1
        b[y0 + 7, :] = 0
        for x in (off % 16, (off + 8) % 16):
            b[y0:y0 + 8, x] = 0
            b[y0:y0 + 7, (x + 1) % 16] = 3 if False else b[y0:y0 + 7, (x + 1) % 16]
    for _ in range(r.randint(1, 3)):
        y, x = r.choice([1, 2, 3, 4, 9, 10, 11, 12]), r.randint(2, 13)
        if b[y, x] == 2:
            b[y, x] = r.choice([1, 3])
    return b


def torch_block():
    b = np.zeros((16, 16), np.uint8)
    # handle / bracket
    b[9:16, 7:9] = 1
    b[9, 6:10] = 2
    b[15, 6:10] = 1
    # flame
    shape = [
        (1, 7, 8), (2, 7, 8), (3, 6, 9), (4, 5, 10), (5, 5, 10),
        (6, 5, 10), (7, 6, 9), (8, 7, 8),
    ]
    for y, x0, x1 in shape:
        b[y, x0:x1 + 1] = 1
    for y, x0, x1 in [(3, 7, 8), (4, 6, 9), (5, 6, 9), (6, 6, 9), (7, 7, 8)]:
        b[y, x0:x1 + 1] = 2
    for y, x0, x1 in [(5, 7, 8), (6, 7, 8), (7, 7, 8)]:
        b[y, x0:x1 + 1] = 3
    return b


def door_block():
    b = np.full((16, 32), 2, np.uint8)
    b[:, 0:2] = 0
    b[:, 30:32] = 0
    b[0:2, :] = 0
    for x in range(2, 30, 6):
        b[2:, x] = 1
    b[5, 2:30] = 1
    b[12, 2:30] = 1
    b[4, 2:30] = 3
    b[11, 2:30] = 3
    b[8:10, 24:26] = 3  # handle
    return b


def put(b, bx, by, pal):
    h, w = b.shape
    idx[by * 16:by * 16 + h, bx * 16:bx * 16 + w] = b
    attr[by:by + h // 16, bx:bx + w // 16] = pal


BW, BH = W // 16, H // 16  # 16 x 15 blocks
for by in range(BH):
    for bx in range(BW):
        border = bx in (0, BW - 1) or by in (0, BH - 1)
        if border:
            put(wall_block(random.Random(bx * 31 + by).randint(0, 3)), bx, by, 1)
        else:
            put(floor_block(random.Random(bx * 7 + by * 13).randint(0, 5)), bx, by, 0)

# Door (2 blocks wide) in top wall, torches either side
put(door_block(), 7, 0, 2)
attr[0, 8] = 2
put(torch_block(), 3, 0, 3)
put(torch_block(), 12, 0, 3)

# Build RGB and validate
rgb = np.zeros((H, W, 3), np.uint8)
for y in range(H):
    for x in range(W):
        p = PALS[attr[y // 16, x // 16]]
        rgb[y, x] = NES[p[idx[y, x]]]

tiles = set()
for ty in range(H // 8):
    for tx in range(W // 8):
        tiles.add(idx[ty * 8:ty * 8 + 8, tx * 8:tx * 8 + 8].tobytes() +
                  bytes([attr[ty // 2, tx // 2]]))
print("unique tiles (approx, incl. palette):", len(tiles))
assert len(tiles) <= 256

Image.fromarray(rgb, "RGB").save("art.png")
