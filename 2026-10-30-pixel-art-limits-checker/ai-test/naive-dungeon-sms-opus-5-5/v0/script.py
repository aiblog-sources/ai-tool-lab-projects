from PIL import Image
import numpy as np

# --- SMS palette (16 colours, each channel in {0,85,170,255}) ---
PAL = [
    (0, 0, 0),        # 0 K   black / mortar
    (0, 0, 85),       # 1 SH  deep shadow
    (85, 85, 85),     # 2 D   floor stone
    (85, 85, 170),    # 3 WS  wall face
    (170, 170, 170),  # 4 G   light grey / iron
    (170, 170, 255),  # 5 WL  wall cap / highlight
    (255, 255, 255),  # 6 W   white highlight
    (85, 0, 0),       # 7 B0  dark wood
    (170, 85, 0),     # 8 B1  wood
    (255, 170, 85),   # 9 B2  light wood
    (255, 0, 0),      # 10 R  flame red
    (255, 170, 0),    # 11 O  flame orange
    (255, 255, 0),    # 12 Y  flame yellow
    (85, 85, 0),      # 13 FD floor grime
    (170, 170, 85),   # 14 FL floor highlight
    (255, 255, 170),  # 15 YL flame core
]
K, SH, D, WS, G, WL, W, B0, B1, B2, R, O, Y, FD, FL, YL = range(16)

Wd, Ht = 256, 192
img = np.zeros((Ht, Wd), dtype=np.uint8)

# ---------- tile pattern builders (all multiples of 8) ----------
def flagstone(variant):
    t = np.full((16, 16), D, np.uint8)
    t[15, :] = K
    t[:, 15] = K
    t[0, :15] = FL
    t[:15, 0] = FL
    t[14, 1:15] = SH
    t[1:15, 14] = SH
    t[0, 0] = G
    rng = np.random.RandomState(7 + variant * 13)
    for _ in range(6):
        y, x = rng.randint(2, 13, 2)
        t[y, x] = FD if rng.rand() < 0.6 else FL
    if variant == 1:  # crack
        for i, (y, x) in enumerate([(3, 4), (4, 5), (5, 5), (6, 6), (7, 7), (8, 7), (9, 8)]):
            t[y, x] = K
    if variant == 2:  # chip
        t[10:12, 10:12] = FD
        t[10, 10] = K
    return t

def wall_face_brick():
    # 16x16 block containing two offset brick rows
    t = np.full((16, 16), WS, np.uint8)
    for row in range(2):
        y0 = row * 8
        t[y0 + 7, :] = K
        t[y0, :] = WL
        t[y0 + 6, :] = SH
        off = 0 if row == 0 else 8
        for x in [(off + 15) % 16]:
            t[y0:y0 + 8, x] = K
            t[y0 + 1:y0 + 7, (x + 1) % 16] = WL
    return t

def cap_block(variant):
    t = np.full((16, 16), WL, np.uint8)
    t[15, :] = K
    t[:, 15] = K
    t[0, :15] = W
    t[:15, 0] = W
    t[14, 1:15] = WS
    t[1:15, 14] = WS
    if variant:
        t[5, 6] = WS
        t[6, 7] = WS
        t[9, 4] = WS
    return t

def blit(t, x, y):
    h, w = t.shape
    img[y:y + h, x:x + w] = t

# ---------- layout ----------
FLOOR_X0, FLOOR_X1 = 16, 240
FLOOR_Y0, FLOOR_Y1 = 40, 176

# floor (offset rows like laid flagstones)
floors = [flagstone(v) for v in range(3)]
for j, y in enumerate(range(FLOOR_Y0, FLOOR_Y1, 16)):
    off = 8 if j % 2 else 0
    for i, x in enumerate(range(FLOOR_X0 - 8, FLOOR_X1, 16)):
        v = (i * 7 + j * 3) % 5
        v = 1 if v == 0 else (2 if v == 3 else 0)
        t = floors[v]
        for yy in range(16):
            for xx in range(16):
                px, py = x + off + xx, y + yy
                if FLOOR_X0 <= px < FLOOR_X1 and FLOOR_Y0 <= py < FLOOR_Y1:
                    img[py, px] = t[yy, xx]

# top wall face (y 16..39)
face = wall_face_brick()
for y in range(16, 40, 16):
    for x in range(0, Wd, 16):
        h = min(16, 40 - y)
        img[y:y + h, x:x + 16] = face[:h]
img[39, :] = K

# caps: top, bottom, sides
caps = [cap_block(0), cap_block(1)]
for i, x in enumerate(range(0, Wd, 16)):
    blit(caps[i % 2], x, 0)
    blit(caps[(i + 1) % 2], x, 176)
for j, y in enumerate(range(16, 176, 16)):
    blit(caps[j % 2], 0, y)
    blit(caps[(j + 1) % 2], 240, y)
# inner edge lines of side/bottom walls
img[16:176, 15] = K
img[16:176, 240] = K
img[176, 16:240] = K

# floor shadows (dither) under top wall and along left wall
for y in range(FLOOR_Y0, FLOOR_Y0 + 6):
    for x in range(FLOOR_X0, FLOOR_X1):
        d = y - FLOOR_Y0
        if d < 2 or (d < 4 and (x + y) % 2 == 0) or (d < 6 and (x + y) % 4 == 0):
            if img[y, x] != K:
                img[y, x] = SH
for x in range(FLOOR_X0, FLOOR_X0 + 4):
    for y in range(FLOOR_Y0, FLOOR_Y1):
        d = x - FLOOR_X0
        if d < 1 or (d < 3 and (x + y) % 2 == 0):
            if img[y, x] != K:
                img[y, x] = SH

# ---------- door (centre of top wall) ----------
DX0, DX1 = 112, 144   # frame outer
DY0 = 6
# stone frame
img[DY0:40, DX0:DX1] = WL
img[DY0:40, DX0] = K
img[DY0:40, DX1 - 1] = K
img[DY0, DX0:DX1] = K
img[DY0 + 1, DX0 + 1:DX1 - 1] = W
img[DY0 + 1:40, DX0 + 1] = W
img[DY0 + 1:40, DX1 - 2] = WS
# keystone
img[DY0:DY0 + 6, 125:131] = WL
img[DY0 + 5, 125:131] = K
img[DY0:DY0 + 6, 124] = K
img[DY0:DY0 + 6, 131] = K
# wooden leaf with arched top
IX0, IX1, IY0 = 116, 140, 12
cx = (IX0 + IX1 - 1) / 2
for y in range(IY0, 40):
    for x in range(IX0, IX1):
        # arch: round off top corners
        dy = y - (IY0 + 8)
        dx = x - cx
        if dy < 0 and dx * dx + (dy * 1.5) ** 2 > 12.5 ** 2:
            continue
        col = (x - IX0) % 6
        if col == 5:
            c = B0
        elif col == 0:
            c = B2
        else:
            c = B1
        img[y, x] = c
# arch outline
for y in range(IY0 - 1, 40):
    row = np.where((img[y, IX0:IX1] == B0) | (img[y, IX0:IX1] == B1) | (img[y, IX0:IX1] == B2))[0]
    if len(row):
        img[y, IX0 + row[0] - 1] = K
        img[y, IX0 + row[-1] + 1] = K
        if y > 0:
            for x in IX0 + row:
                if img[y - 1, x] not in (B0, B1, B2):
                    img[y - 1, x] = K
# iron bands
for by in (19, 32):
    for x in range(IX0, IX1):
        if img[by, x] in (B0, B1, B2):
            img[by, x] = G
            img[by + 1, x] = D
    for x in (IX0 + 2, IX1 - 3):
        img[by, x] = W
# ring handle
for (y, x) in [(25, 133), (25, 135), (26, 132), (26, 136), (27, 132), (27, 136), (28, 133), (28, 135), (24, 134)]:
    img[y, x] = Y
img[29, 134] = O
# threshold step on floor
img[40:43, 114:142] = G
img[42, 114:142] = D
img[43, 114:142] = K

# ---------- torches ----------
FLAME = [
    "....Y...",
    "...YY...",
    "..YOY...",
    "..OYYO..",
    ".OYLYO..",
    ".ROLLOR.",
    ".ROYYOR.",
    "..RROR..",
]
cmap = {"Y": Y, "O": O, "R": R, "L": YL}

def torch(x0):
    y0 = 14
    # warm glow on the wall (dithered)
    for y in range(y0, 40):
        for x in range(x0 - 8, x0 + 16):
            dx = x - (x0 + 3.5)
            dy = y - (y0 + 6)
            r2 = dx * dx + dy * dy * 0.8
            if r2 < 120 and img[y, x] == WS and (x + y) % 2 == 0:
                img[y, x] = O if r2 < 30 else B1
            elif r2 < 120 and img[y, x] == WL and r2 < 60:
                img[y, x] = B2
    # flame
    for j, row in enumerate(FLAME):
        for i, ch in enumerate(row):
            if ch in cmap:
                img[y0 + j, x0 + i] = cmap[ch]
    # cup and stick
    img[y0 + 8, x0 + 1:x0 + 7] = G
    img[y0 + 9, x0 + 1:x0 + 7] = D
    img[y0 + 8, x0 + 1] = W
    for y in range(y0 + 10, y0 + 18):
        img[y, x0 + 3] = B2
        img[y, x0 + 4] = B1
    img[y0 + 17, x0 + 3:x0 + 5] = B0
    # wall bracket
    img[y0 + 14, x0 + 1:x0 + 7] = K
    img[y0 + 15, x0 + 1:x0 + 7] = G
    img[y0 + 16, x0 + 2:x0 + 6] = D
    img[y0 + 13:y0 + 17, x0 + 2] = K
    img[y0 + 13:y0 + 17, x0 + 5] = K

torch(60)
torch(188)

# warm light pools on the floor beneath torches
for cxp in (64, 192):
    for y in range(FLOOR_Y0, FLOOR_Y0 + 14):
        for x in range(cxp - 14, cxp + 14):
            dx, dy = x - cxp, (y - FLOOR_Y0) * 1.6
            if dx * dx + dy * dy < 150 and img[y, x] in (D, SH) and (x + y) % 2 == 0:
                img[y, x] = FD if dx * dx + dy * dy > 60 else FL

# ---------- save as indexed PNG ----------
out = Image.fromarray(img, mode="P")
flat = [c for rgb in PAL for c in rgb]
flat += [0] * (768 - len(flat))
out.putpalette(flat)
out.save("art.png")
