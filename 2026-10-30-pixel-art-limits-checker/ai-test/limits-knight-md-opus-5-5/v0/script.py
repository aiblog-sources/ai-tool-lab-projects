#!/usr/bin/env python3
# Small knight walk cycle (4 frames of 32x32, facing right) for Sega Mega Drive.
# Hardware constraints respected:
#  - Single 16-entry palette (one CRAM line); index 0 = transparent, 13 visible colors used.
#  - Every color is on the MD 9-bit RGB grid (3 bits/channel: 0x00,0x24,...,0xFF).
#  - Each frame is 32x32 = 4x4 tiles, the largest single hardware sprite size,
#    so every frame is exactly one sprite (16 tiles, 64 tiles for the whole sheet).
#  - Output is an indexed PNG with the palette in hardware order, ready for tools like SGDK rescomp.
from PIL import Image
import numpy as np

L = [0x00, 0x24, 0x49, 0x6D, 0x92, 0xB6, 0xDB, 0xFF]  # MD channel levels
PAL = [
    (0xFF, 0x00, 0xFF),  # 0 transparent
    (0x24, 0x24, 0x49),  # 1 outline
    (0x6D, 0x6D, 0x92),  # 2 steel dark
    (0xB6, 0xB6, 0xDB),  # 3 steel mid
    (0xFF, 0xFF, 0xFF),  # 4 steel light
    (0x92, 0x00, 0x24),  # 5 red dark
    (0xDB, 0x24, 0x24),  # 6 red
    (0xFF, 0xB6, 0x92),  # 7 skin (unused spare)
    (0x6D, 0x49, 0x24),  # 8 brown dark
    (0x92, 0x6D, 0x49),  # 9 brown
    (0xDB, 0xB6, 0x24),  # 10 gold
    (0x24, 0x49, 0x92),  # 11 blue dark
    (0x49, 0x6D, 0xDB),  # 12 blue
    (0x00, 0x00, 0x00),  # 13 black
    (0x00, 0x00, 0x00),  # 14 unused
    (0x00, 0x00, 0x00),  # 15 unused
]
for c in PAL:
    assert all(v in L for v in c), c

W = H = 32


def make_frame(f):
    a = np.zeros((H, W), dtype=np.uint8)

    def px(x, y, c):
        if 1 <= x < W - 1 and 1 <= y < H - 1:
            a[y, x] = c

    def rect(x0, y0, x1, y1, c):
        for y in range(y0, y1 + 1):
            for x in range(x0, x1 + 1):
                px(x, y, c)

    bob = -1 if f in (1, 3) else 0
    arm = [-1, 0, 1, 0][f]
    # (hip_x, foot_x, foot_lift) for back and front legs
    back = [(13, 10, 0), (13, 14, 2), (13, 18, 0), (13, 13, 0)][f]
    front = [(16, 19, 0), (16, 15, 0), (16, 12, 0), (16, 16, 2)][f]

    def leg(hx, fx, lift, main, shade, boot, boot_s):
        hy = 21 + bob
        fy = 27 - lift
        steps = max(abs(fx - hx), fy - hy) * 2
        for i in range(steps + 1):
            t = i / steps
            x = round(hx + (fx - hx) * t)
            y = round(hy + (fy - hy) * t)
            px(x - 1, y, shade)
            px(x, y, main)
            px(x + 1, y, main)
        by = 28 - lift
        rect(fx - 1, by, fx + 2, by + 2, boot)
        rect(fx - 1, by + 2, fx + 2, by + 2, boot_s)
        px(fx + 2, by, 0)  # rounded toe

    # back leg (darker), then shield behind body
    leg(*back, 2, 1, 8, 8)

    oy = bob
    # shield (kite) on the far arm
    rows = [(7, 11), (7, 11), (7, 11), (7, 11), (7, 11), (7, 11), (8, 11), (8, 10), (9, 10), (9, 9)]
    for i, (x0, x1) in enumerate(rows):
        y = 12 + i + oy
        for x in range(x0, x1 + 1):
            edge = x in (x0, x1) or i == 0 or i == len(rows) - 1
            px(x, y, 10 if edge else 6)
        px(x0 + (1 if i not in (0, len(rows) - 1) else 0), y, 10 if i in (0, len(rows) - 1) else 5)
    rect(9, 13 + oy, 9, 18 + oy, 10)
    rect(8, 15 + oy, 10, 15 + oy, 10)

    # front leg
    leg(*front, 3, 2, 9, 8)

    # torso / tabard
    rect(12, 12 + oy, 18, 19 + oy, 12)
    rect(12, 12 + oy, 12, 19 + oy, 11)
    rect(13, 12 + oy, 17, 12 + oy, 3)  # mail collar
    rect(12, 18 + oy, 18, 18 + oy, 8)  # belt
    px(17, 18 + oy, 10)  # buckle
    rect(12, 20 + oy, 18, 21 + oy, 12)  # skirt
    rect(12, 20 + oy, 12, 21 + oy, 11)
    for x in (14, 16):
        px(x, 21 + oy, 11)
    # tabard emblem
    px(15, 14 + oy, 10); px(15, 15 + oy, 10); px(14, 15 + oy, 10); px(16, 15 + oy, 10); px(15, 16 + oy, 10)

    # helmet
    rect(11, 5 + oy, 19, 11 + oy, 3)
    rect(12, 4 + oy, 18, 4 + oy, 3)
    px(11, 5 + oy, 0); px(19, 5 + oy, 0)
    rect(12, 5 + oy, 12, 9 + oy, 4)  # highlight
    rect(13, 4 + oy, 15, 4 + oy, 4)
    rect(11, 10 + oy, 19, 11 + oy, 2)  # lower shade / gorget
    rect(15, 7 + oy, 19, 8 + oy, 13)  # visor slit
    px(15, 8 + oy, 2)
    px(17, 10 + oy, 13); px(18, 10 + oy, 13)  # breathing holes
    # plume, sways slightly with walk
    sway = [0, 1, 0, 1][f]
    for (x, y, c) in [(14, 3, 6), (13, 2, 6), (12, 2, 6), (11, 2, 6), (10, 3, 6),
                      (9, 4 + sway, 5), (8, 5 + sway, 5), (11, 3, 5), (12, 3, 6), (13, 3, 6)]:
        px(x, y + oy, c)

    # sword arm (near side)
    ax = 19 + arm
    rect(18, 13 + oy, 19, 14 + oy, 3)  # pauldron
    rect(19, 13 + oy, 19, 13 + oy, 4)
    rect(ax, 15 + oy, ax + 1, 16 + oy, 3)
    rect(ax + 1, 17 + oy, ax + 3, 18 + oy, 3)  # gauntlet
    px(ax + 1, 18 + oy, 2)
    sx = ax + 2
    rect(sx, 5 + oy, sx, 15 + oy, 4)   # blade light edge
    rect(sx + 1, 5 + oy, sx + 1, 15 + oy, 3)
    px(sx, 4 + oy, 4)                  # tip
    rect(sx - 2, 16 + oy, sx + 3, 16 + oy, 10)  # crossguard
    px(sx + 3, 16 + oy, 8)
    px(sx, 19 + oy, 9); px(sx + 1, 19 + oy, 9)  # grip below hand
    px(sx, 20 + oy, 10)                         # pommel

    # automatic 1px outline around the silhouette
    out = a.copy()
    for y in range(H):
        for x in range(W):
            if a[y, x] == 0:
                for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                    nx, ny = x + dx, y + dy
                    if 0 <= nx < W and 0 <= ny < H and a[ny, nx] != 0:
                        out[y, x] = 1
                        break
    return out


sheet = np.zeros((H, W * 4), dtype=np.uint8)
for f in range(4):
    sheet[:, f * W:(f + 1) * W] = make_frame(f)

assert sheet.max() < 16
img = Image.fromarray(sheet, mode="P")
flat = []
for c in PAL:
    flat.extend(c)
img.putpalette(flat)
img.info["transparency"] = 0
img.save("art.png", transparency=0)
