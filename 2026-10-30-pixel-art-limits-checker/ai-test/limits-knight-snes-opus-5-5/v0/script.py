import numpy as np
from PIL import Image, ImageDraw

# SNES-safe palette (BGR555: every channel a multiple of 8). Index 0 = transparent.
PAL = [
    (0, 0, 0),        # 0 transparent
    (24, 16, 32),     # 1 outline
    (72, 80, 104),    # 2 steel dark
    (128, 136, 160),  # 3 steel mid
    (192, 200, 216),  # 4 steel light
    (240, 240, 248),  # 5 highlight
    (128, 24, 32),    # 6 plume dark
    (200, 48, 48),    # 7 plume
    (96, 56, 32),     # 8 leather dark
    (152, 96, 48),    # 9 leather light
    (216, 176, 56),   # 10 gold
    (32, 48, 112),    # 11 cape dark
    (56, 88, 176),    # 12 cape
    (0, 0, 0), (0, 0, 0), (0, 0, 0),  # unused
]
for c in PAL:
    assert all(v % 8 == 0 for v in c)

# (near_foot_x, near_lift, far_foot_x, far_lift)
POSES = [(19, 0, 10, 0), (15, 0, 13, 2), (10, 0, 19, 0), (13, 2, 15, 0)]
ARM = [-1, 0, 1, 0]
CAPE_TAIL = [(6, 23), (5, 22), (6, 23), (7, 22)]
PLUME_WAVE = [0, 1, 0, -1]


def leg(d, hx, hy, fx, lift, col, hi):
    ay = 27 - lift
    for dx in (-1, 0, 1):
        d.line((hx + dx, hy, fx + dx, ay), fill=col)
    d.line((fx - 1, (hy + ay) // 2, fx - 1, (hy + ay) // 2), fill=hi)  # knee glint
    d.rectangle((fx - 1, ay + 1, fx + 2, ay + 1), fill=9)  # boot
    d.rectangle((fx - 1, ay + 2, fx + 2, ay + 2), fill=8)  # sole


def frame(i):
    im = Image.new("P", (32, 32), 0)
    d = ImageDraw.Draw(im)
    b = 0 if i % 2 == 0 else -1
    nfx, nl, ffx, fl = POSES[i]
    ax = ARM[i]

    # Cape (behind)
    tx, ty = CAPE_TAIL[i]
    d.polygon([(11, 13 + b), (9, 14 + b), (tx, ty + b), (11, 22 + b)], fill=12)
    d.line((9, 14 + b, tx, ty + b), fill=11)

    # Far leg
    leg(d, 14, 21 + b, ffx, fl, 2, 3)

    # Torso
    d.rectangle((11, 13 + b, 19, 19 + b), fill=3)
    d.rectangle((11, 13 + b, 12, 19 + b), fill=2)
    d.line((18, 14 + b, 18, 18 + b), fill=4)
    d.rectangle((11, 20 + b, 19, 20 + b), fill=8)
    d.point((16, 20 + b), fill=10)
    d.rectangle((12, 21 + b, 18, 21 + b), fill=2)

    # Near leg
    leg(d, 16, 21 + b, nfx, nl, 3, 4)

    # Helmet
    d.rectangle((12, 6 + b, 19, 12 + b), fill=3)
    d.rectangle((13, 5 + b, 18, 5 + b), fill=3)
    d.line((12, 7 + b, 12, 12 + b), fill=2)
    d.rectangle((17, 6 + b, 18, 7 + b), fill=4)
    d.point((17, 6 + b), fill=5)
    d.line((15, 9 + b, 19, 9 + b), fill=1)
    d.point((17, 11 + b), fill=2)
    d.point((19, 11 + b), fill=2)
    d.line((13, 12 + b, 18, 12 + b), fill=2)

    # Plume
    pw = PLUME_WAVE[i]
    d.polygon([(16, 5 + b), (15, 2 + b), (12, 2 + b), (9, 3 + b + pw),
               (8, 5 + b + pw), (12, 4 + b)], fill=7)
    d.line((8, 5 + b + pw, 12, 5 + b), fill=6)

    # Sword
    hx = 21 + ax
    d.rectangle((hx, 7 + b, hx, 15 + b), fill=5)
    d.rectangle((hx + 1, 7 + b, hx + 1, 15 + b), fill=3)
    d.point((hx, 6 + b), fill=4)
    d.rectangle((hx - 2, 16 + b, hx + 3, 16 + b), fill=10)
    d.rectangle((hx, 17 + b, hx + 1, 18 + b), fill=8)
    d.rectangle((hx, 19 + b, hx + 1, 19 + b), fill=10)

    # Arm, pauldron, gauntlet
    d.line((15, 15 + b, hx - 1, 17 + b), fill=3, width=2)
    d.ellipse((13, 13 + b, 18, 16 + b), fill=3)
    d.line((14, 13 + b, 17, 13 + b), fill=4)
    d.rectangle((hx - 1, 17 + b, hx, 18 + b), fill=2)

    # Auto outline
    g = np.array(im, dtype=np.uint8)
    m = g > 0
    n = np.zeros_like(m)
    n[1:, :] |= m[:-1, :]
    n[:-1, :] |= m[1:, :]
    n[:, 1:] |= m[:, :-1]
    n[:, :-1] |= m[:, 1:]
    g[n & ~m] = 1
    return g


sheet = np.zeros((32, 128), dtype=np.uint8)
for i in range(4):
    sheet[:, i * 32:(i + 1) * 32] = frame(i)

assert sheet.max() <= 15
out = Image.fromarray(sheet, mode="P")
flat = [v for c in PAL for v in c]
out.putpalette(flat)
out.save("art.png", transparency=0, bits=4)
