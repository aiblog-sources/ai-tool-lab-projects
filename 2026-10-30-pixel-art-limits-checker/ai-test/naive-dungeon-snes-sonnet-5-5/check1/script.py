from PIL import Image, ImageDraw
import random

W, H = 256, 224
random.seed(7)

# Single global 15-colour palette (all channels multiples of 8 -> exact in 15-bit RGB)
PAL = [
    (24, 16, 24),     # 0 near-black
    (56, 48, 64),     # 1 dark stone / lines
    (72, 64, 80),     # 2 stone dark
    (88, 80, 96),     # 3 stone mid
    (104, 96, 112),   # 4 stone light
    (120, 112, 128),  # 5 stone highlight
    (120, 72, 40),    # 6 wood light
    (96, 56, 32),     # 7 wood mid
    (64, 40, 24),     # 8 wood dark
    (72, 72, 80),     # 9 iron
    (208, 168, 56),   # 10 gold
    (216, 88, 24),    # 11 flame orange
    (248, 168, 40),   # 12 flame yellow
    (248, 240, 152),  # 13 flame core
    (144, 144, 160),  # 14 metal highlight
]
RAMP = [PAL[1], PAL[2], PAL[3], PAL[4], PAL[5]]

img = Image.new("RGB", (W, H), PAL[0])
d = ImageDraw.Draw(img)

FLOOR = [PAL[3], PAL[4], PAL[2]]
FLOOR_LINE = PAL[1]
WALL = [PAL[2], PAL[3], PAL[2]]
WALL_DARK = PAL[1]
WALL_LIGHT = PAL[5]
WOOD = [PAL[6], PAL[7], PAL[8]]
IRON = PAL[9]

WALL_TOP = 64
SIDE = 24
BOT = 24

ts = 16
for y in range(WALL_TOP, H - BOT, ts):
    for x in range(SIDE, W - SIDE, ts):
        c = random.choice(FLOOR)
        d.rectangle([x, y, x + ts - 1, y + ts - 1], fill=c)
        d.line([x, y, x + ts - 1, y], fill=FLOOR_LINE)
        d.line([x, y, x, y + ts - 1], fill=FLOOR_LINE)
        d.point((x + 1, y + 1), fill=PAL[4])
        for _ in range(3):
            px, py = x + random.randint(2, 13), y + random.randint(2, 13)
            d.point((px, py), fill=random.choice([PAL[2], PAL[4]]))

def brick_area(x0, y0, x1, y1, bw=16, bh=8):
    d.rectangle([x0, y0, x1, y1], fill=WALL[0])
    row = 0
    for y in range(y0, y1 + 1, bh):
        off = (bw // 2) if row % 2 else 0
        for x in range(x0 - off, x1 + 1, bw):
            bx0, bx1 = max(x, x0), min(x + bw - 1, x1)
            by1 = min(y + bh - 1, y1)
            if bx0 > bx1:
                continue
            d.rectangle([bx0, y, bx1, by1], fill=random.choice(WALL))
            d.line([bx0, y, bx1, y], fill=WALL_LIGHT)
            d.line([bx0, by1, bx1, by1], fill=WALL_DARK)
            if x >= x0:
                d.line([x, y, x, by1], fill=WALL_DARK)
        row += 1

brick_area(0, 0, W - 1, WALL_TOP - 1)
brick_area(0, WALL_TOP, SIDE - 1, H - 1)
brick_area(W - SIDE, WALL_TOP, W - 1, H - 1)
brick_area(SIDE, H - BOT, W - SIDE - 1, H - 1)

# Wall cap / shadow on floor (palette colours only)
d.line([SIDE, WALL_TOP - 1, W - SIDE - 1, WALL_TOP - 1], fill=WALL_LIGHT)
shadow_cols = [PAL[2], PAL[2], PAL[1], PAL[1]]
for i in range(4):
    d.line([SIDE, WALL_TOP + i, W - SIDE - 1, WALL_TOP + i], fill=shadow_cols[i])
for i in range(3):
    c = [PAL[2], PAL[1], PAL[1]][i]
    d.line([SIDE + i, WALL_TOP, SIDE + i, H - BOT - 1], fill=c)
    d.line([W - SIDE - 1 - i, WALL_TOP, W - SIDE - 1 - i, H - BOT - 1], fill=c)

# Door in north wall
dw, dh = 40, 52
dx = (W - dw) // 2
dy = WALL_TOP - dh
d.rectangle([dx - 4, dy - 4, dx + dw + 3, WALL_TOP - 1], fill=PAL[1])
d.rectangle([dx - 2, dy - 2, dx + dw + 1, WALL_TOP - 1], fill=PAL[4])
d.rectangle([dx, dy, dx + dw - 1, WALL_TOP - 1], fill=WOOD[1])
d.rectangle([dx, dy, dx + dw - 1, dy + 1], fill=WOOD[2])
for px in range(dx, dx + dw, 8):
    d.rectangle([px, dy + 2, px + 6, WALL_TOP - 1], fill=WOOD[0])
    d.line([px + 7, dy + 2, px + 7, WALL_TOP - 1], fill=WOOD[2])
    d.line([px + 1, dy + 2, px + 1, WALL_TOP - 1], fill=WOOD[0])
for by in (dy + 10, dy + 36):
    d.rectangle([dx, by, dx + dw - 1, by + 3], fill=IRON)
    d.line([dx, by, dx + dw - 1, by], fill=PAL[14])
    for rx in range(dx + 3, dx + dw, 12):
        d.point((rx, by + 1), fill=PAL[14])
d.rectangle([dx + dw - 9, dy + 26, dx + dw - 6, dy + 29], fill=PAL[10])
d.point((dx + dw - 9, dy + 26), fill=PAL[13])
d.rectangle([dx - 4, WALL_TOP, dx + dw + 3, WALL_TOP + 2], fill=PAL[5])

# Snap everything to the palette
def quantize(im):
    px = im.load()
    cache = {}
    for y in range(H):
        for x in range(W):
            c = px[x, y]
            if c not in cache:
                cache[c] = min(PAL, key=lambda p: (p[0]-c[0])**2 + (p[1]-c[1])**2 + (p[2]-c[2])**2)
            px[x, y] = cache[c]

quantize(img)

# Dithered torch glow: step stone pixels one notch up the grey ramp
TORCHES = [(68, 36), (188, 36)]
px = img.load()
for tx, ty in TORCHES:
    R = 44
    for y in range(max(0, ty - R), min(H, ty + R)):
        for x in range(max(0, tx - R), min(W, tx + R)):
            dist = ((x - tx) ** 2 + (y - ty) ** 2) ** 0.5
            if dist >= R:
                continue
            c = px[x, y]
            if c in RAMP:
                i = RAMP.index(c)
                if i < len(RAMP) - 1:
                    strong = dist < R * 0.5
                    if ((x + y) % 2 == 0 and strong) or ((x % 2 == 0 and y % 2 == 0) and not strong):
                        px[x, y] = RAMP[i + 1]

d = ImageDraw.Draw(img)

def torch(cx, cy):
    d.rectangle([cx - 3, cy + 6, cx + 2, cy + 8], fill=IRON)
    d.rectangle([cx - 2, cy, cx + 1, cy + 10], fill=WOOD[1])
    d.line([cx - 2, cy, cx - 2, cy + 10], fill=WOOD[0])
    d.rectangle([cx - 3, cy - 2, cx + 2, cy + 1], fill=IRON)
    d.polygon([(cx - 3, cy - 2), (cx - 1, cy - 8), (cx, cy - 13), (cx + 1, cy - 8), (cx + 3, cy - 2)], fill=PAL[11])
    d.polygon([(cx - 2, cy - 2), (cx, cy - 10), (cx + 2, cy - 2)], fill=PAL[12])
    d.polygon([(cx - 1, cy - 2), (cx, cy - 7), (cx + 1, cy - 2)], fill=PAL[13])

for tx, ty in TORCHES:
    torch(tx, ty)

# Cracks
for cx, cy in ((90, 150), (170, 120), (60, 110)):
    pts = [(cx, cy)]
    for _ in range(5):
        cx += random.randint(-4, 5)
        cy += random.randint(1, 5)
        pts.append((cx, cy))
    d.line(pts, fill=FLOOR_LINE)

quantize(img)
img.save("art.png")
