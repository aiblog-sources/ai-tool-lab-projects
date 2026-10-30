#!/usr/bin/env python3
# Sunset over mountains and a lake: a 320x224 Mega Drive / Genesis background.
# Uses one 16-colour palette, with every colour snapped to the MD 9-bit grid
# (3 bits per channel, levels 0..7 * 36). That keeps every 8x8 tile valid.
import math
import random
import numpy as np
from PIL import Image

W, H = 320, 224
HORIZON = 150
random.seed(7)

PAL = [
    (36, 36, 108),    # 0 indigo (sky top)
    (72, 36, 108),    # 1 dark purple
    (108, 72, 144),   # 2 purple
    (180, 72, 144),   # 3 magenta
    (216, 108, 108),  # 4 pink-orange
    (252, 144, 72),   # 5 orange
    (252, 216, 108),  # 6 yellow
    (252, 252, 180),  # 7 sun core
    (144, 72, 108),   # 8 far mountains
    (108, 36, 108),   # 9 mid mountains
    (72, 36, 72),     # 10 near hills
    (36, 0, 36),      # 11 silhouettes
    (36, 36, 72),     # 12 deep water
    (216, 108, 72),   # 13 warm rim light
    (252, 252, 252),  # 14 sparkle / stars
    (180, 108, 144),  # 15 cool rim light
]
for c in PAL:
    assert all(v % 36 == 0 and v <= 252 for v in c)

img = np.zeros((H, W), dtype=np.uint8)

# ---------------- Sky: banded gradient with ordered-dither seams -------------
BAYER = np.array([[0, 8, 2, 10], [12, 4, 14, 6],
                  [3, 11, 1, 9], [15, 7, 13, 5]]) / 16.0
bands = [0, 1, 2, 3, 4, 5, 6]
for y in range(HORIZON):
    t = (y / (HORIZON - 1)) ** 1.15 * (len(bands) - 1)
    base = min(int(t), len(bands) - 2)
    frac = t - base
    f2 = min(max((frac - 0.6) / 0.4, 0.0), 1.0)  # solid band, dithered edge
    for x in range(W):
        img[y, x] = bands[base + 1] if f2 > BAYER[y % 4, x % 4] else bands[base]

# Stars in the upper sky
for _ in range(28):
    x, y = random.randrange(W), random.randrange(2, 34)
    img[y, x] = 14 if random.random() < 0.4 else 15

# ---------------- Clouds: long thin streaks with lit undersides --------------
clouds = [
    (60, 46, 46, 3, 1, 3), (250, 38, 58, 3, 1, 3), (150, 62, 40, 2, 2, 4),
    (40, 80, 54, 3, 3, 5), (275, 74, 44, 3, 3, 5), (110, 92, 34, 2, 4, 6),
    (215, 100, 40, 2, 4, 6),
]
for cx, cy, cw, ch, col, lit in clouds:
    for y in range(cy - ch, cy + ch + 1):
        dy = (y - cy) / (ch + 0.5)
        half = int(cw * math.sqrt(max(0.0, 1 - dy * dy)))
        for x in range(cx - half, cx + half + 1):
            if 0 <= x < W and 0 <= y < HORIZON:
                img[y, x] = lit if y == cy + ch else col

# ---------------- Sun with retro cut lines -----------------------------------
SCX, SCY, SR = 160, 126, 26
gaps = {SCY + g for g in (5, 10, 14, 17, 20, 22, 24)}
for y in range(SCY - SR, SCY + SR + 1):
    for x in range(SCX - SR, SCX + SR + 1):
        d = math.hypot(x - SCX, y - SCY)
        if d <= SR and y < HORIZON and y not in gaps:
            img[y, x] = 7 if d < SR - 2.2 else 6

# ---------------- Far mountain range -----------------------------------------
def tri(v):
    return abs((v % 2.0) - 1.0)

far_h = []
for x in range(W):
    hgt = (34 + 14 * math.sin(x * 0.027 + 1.3) + 9 * math.sin(x * 0.069 + 2.1)
           + 10 * tri(x / 23.0) + 4 * tri(x / 7.0))
    far_h.append(int(HORIZON - hgt))
for x in range(W):
    top = far_h[x]
    for y in range(top, HORIZON):
        img[y, x] = 8
    img[top, x] = 13 if abs(x - SCX) < 90 else 15
    # Faces turned toward the sun pick up a second rim pixel
    if 0 < x < W - 1:
        facing = (x < SCX and far_h[x + 1] > top) or (x > SCX and far_h[x - 1] > top)
        if facing and top + 1 < HORIZON:
            img[top + 1, x] = 13 if abs(x - SCX) < 90 else 15

# ---------------- Mid mountains (bigger at the sides) ------------------------
mid_h = []
for x in range(W):
    side = abs(x - SCX) / SCX
    hgt = 6 + 58 * side ** 1.6 + 6 * math.sin(x * 0.11) + 7 * tri(x / 17.0)
    mid_h.append(int(HORIZON - hgt))
for x in range(W):
    for y in range(mid_h[x], HORIZON):
        img[y, x] = 9
    # Dithered shading on the faces turned away from the light
    if 0 < x < W - 1:
        away = (x < SCX and mid_h[x + 1] < mid_h[x]) or (x > SCX and mid_h[x - 1] < mid_h[x])
        if away:
            for y in range(mid_h[x], HORIZON):
                if (x + y) % 2 == 0:
                    img[y, x] = 10
    img[mid_h[x], x] = 8  # soft top edge

# ---------------- Near hills and pine silhouettes ----------------------------
near_h = []
for x in range(W):
    side = max(0.0, (abs(x - SCX) - 70) / 90)
    hgt = 3 + 16 * side ** 1.3 + 2 * math.sin(x * 0.2)
    near_h.append(int(HORIZON - hgt))
for x in range(W):
    for y in range(near_h[x], HORIZON):
        img[y, x] = 10

def pine(bx, by, h):
    for r in range(h):
        y = by - h + r
        tier = (r % 5) / 5.0
        half = int((r * 0.32) * (0.6 + 0.4 * tier)) + (1 if r > 1 else 0)
        for x in range(bx - half, bx + half + 1):
            if 0 <= x < W and 0 <= y < HORIZON:
                img[y, x] = 11
    for y in range(by - 2, by + 1):  # trunk
        if 0 <= bx < W and y < HORIZON:
            img[y, bx] = 11

for bx in list(range(2, 80, 7)) + list(range(244, 320, 6)):
    bx += random.randint(-2, 2)
    if 0 <= bx < W:
        pine(bx, near_h[bx] + 3, random.randint(12, 24) if abs(bx - SCX) > 110 else random.randint(7, 12))

# Shoreline strip
img[HORIZON - 1, :] = np.where(img[HORIZON - 1, :] >= 8, 11, img[HORIZON - 1, :])

# ---------------- Lake: rippled, darkened reflection -------------------------
DARK = {0: 12, 1: 0, 2: 1, 3: 2, 4: 3, 5: 4, 6: 5, 7: 6, 8: 9, 9: 10,
        10: 11, 11: 11, 12: 12, 13: 3, 14: 2, 15: 9}
lake_rows = H - HORIZON
for y in range(HORIZON, H):
    d = y - HORIZON
    src = HORIZON - 1 - d
    depth = d / lake_rows
    amp = 1 + 3 * depth
    off = int(round(amp * math.sin(y * 0.9 + d * 0.15)))
    for x in range(W):
        sx = min(max(x + off, 0), W - 1)
        c = DARK[int(img[max(src, 0), sx])]
        if depth > 0.55 and (x + y) % 2 == 0:  # darker, dithered far side
            c = DARK[c]
        img[y, x] = c

# Horizontal ripple lines across the water
for y in range(HORIZON + 2, H, 3):
    x = random.randrange(-20, W)
    while x < W:
        length = random.randint(6, 26)
        col = 12 if random.random() < 0.7 else 2
        for xx in range(max(x, 0), min(x + length, W)):
            if img[y, xx] not in (5, 6, 7):
                img[y, xx] = col
        x += length + random.randint(10, 40)

# Sun glitter path
for y in range(HORIZON + 1, H):
    d = y - HORIZON
    spread = 10 + d * 0.9
    if d % 2 == 0:
        x = int(SCX - spread + random.randint(0, 4))
        while x < SCX + spread:
            length = random.randint(2, 7)
            col = 7 if abs(x - SCX) < spread * 0.35 else (6 if random.random() < 0.6 else 5)
            for xx in range(x, min(x + length, W)):
                if 0 <= xx < W:
                    img[y, xx] = col
            x += length + random.randint(2, 6) + d // 10
for _ in range(14):
    x, y = random.randint(SCX - 40, SCX + 40), random.randint(HORIZON + 3, H - 20)
    img[y, x] = 14

# ---------------- Foreground: reeds and bank in the bottom corners ------------
for x in range(W):
    edge = abs(x - SCX) / SCX
    bank = int(H - 1 - max(0, (edge - 0.55)) * 40 - 2 * math.sin(x * 0.3))
    for y in range(bank, H):
        img[y, x] = 11
for _ in range(40):
    x = random.choice([random.randint(0, 60), random.randint(262, W - 1)])
    edge = abs(x - SCX) / SCX
    base = int(H - 1 - max(0, (edge - 0.55)) * 40)
    h = random.randint(6, 18)
    lean = random.choice([-1, 0, 1])
    for i in range(h):
        xx = x + (lean * i) // 6
        yy = base - i
        if 0 <= xx < W and HORIZON < yy < H:
            img[yy, xx] = 11
    if random.random() < 0.35:  # cattail head
        for i in range(h - 3, h):
            xx, yy = x + (lean * i) // 6, base - i
            if 0 <= xx < W and HORIZON < yy < H:
                img[yy, xx] = 11
                if xx + 1 < W:
                    img[yy, xx + 1] = 11

# ---------------- Save as an indexed 16-colour image --------------------------
out = Image.fromarray(img, mode="P")
flat = [v for c in PAL for v in c]
flat += [0] * (768 - len(flat))
out.putpalette(flat)
out.save("art.png")
print("Saved art.png (320x224, 16 colours, Mega Drive 9-bit palette)")
