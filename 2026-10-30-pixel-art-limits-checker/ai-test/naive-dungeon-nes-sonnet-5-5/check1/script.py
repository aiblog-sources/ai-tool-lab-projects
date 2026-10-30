from PIL import Image, ImageDraw

W, H = 256, 240

# NES-style palette (11 colors). GRY2 is the shared backdrop color.
BLK  = (0, 0, 0)
DK   = (44, 44, 48)
GRY1 = (84, 84, 84)
GRY2 = (124, 124, 124)   # backdrop
GRY3 = (170, 170, 170)
BRN1 = (72, 36, 8)
BRN2 = (136, 80, 16)
BRN3 = (188, 124, 40)
RED  = (200, 40, 24)
ORG  = (252, 152, 56)
YEL  = (252, 224, 120)

img = Image.new("RGB", (W, H), GRY2)
d = ImageDraw.Draw(img)

def rect(x0, y0, x1, y1, c):
    d.rectangle([x0, y0, x1, y1], fill=c)

TOP, BOT, SIDE = 48, 16, 16
fx0, fy0, fx1, fy1 = SIDE, TOP, W - SIDE - 1, H - BOT - 1

# ---------- Walls: staggered bricks (palette: BLK, GRY1, DK + backdrop) ----------
rect(0, 0, W - 1, H - 1, BLK)
for row in range(H // 8):
    y = row * 8
    off = 8 if row % 2 else 0
    for x in range(-16 + off, W + 16, 16):
        rect(x + 1, y + 1, x + 14, y + 6, GRY1)   # face
        rect(x + 1, y + 1, x + 14, y + 1, GRY2)   # top highlight (backdrop)
        rect(x + 1, y + 6, x + 14, y + 6, DK)     # bottom shade

# ---------- Floor: identical 16x16 slabs (palette: GRY2 backdrop, GRY3, DK) ----------
cracks = {(2, 2), (9, 4), (5, 7), (11, 9), (1, 8)}
for r, sy in enumerate(range(fy0, fy1 + 1, 16)):
    for c, sx in enumerate(range(fx0, fx1 + 1, 16)):
        rect(sx, sy, sx + 15, sy + 15, GRY2)
        rect(sx, sy, sx + 15, sy, GRY3)
        rect(sx, sy, sx, sy + 15, GRY3)
        rect(sx + 15, sy + 1, sx + 15, sy + 15, DK)
        rect(sx + 1, sy + 15, sx + 15, sy + 15, DK)
        if (c, r) in cracks:
            for i in range(5):
                d.point((sx + 5 + i, sy + 6 + i // 2), fill=DK)
            d.point((sx + 9, sy + 9), fill=DK)
            d.point((sx + 10, sy + 10), fill=DK)

# Wall shadow cast onto floor (top and sides)
for y in range(fy0, fy0 + 4):
    i = y - fy0
    for x in range(fx0, fx1 + 1):
        if i < 2 or (x + y) % 2 == 0:
            d.point((x, y), fill=DK)
for y in range(fy0 + 4, fy1 + 1):
    for x in (fx0, fx0 + 1, fx1 - 1, fx1):
        d.point((x, y), fill=DK)
    for x in (fx0 + 2, fx1 - 2):
        if (x + y) % 2 == 0:
            d.point((x, y), fill=DK)

# ---------- Door (wood palette: BRN1, BRN2, BRN3) ----------
dx0, dx1 = 112, 143
dy0, dy1 = 16, 47
# stone frame columns (palette: BLK, GRY3, GRY2 backdrop)
for fx_ in (96, 144):
    rect(fx_, dy0, fx_ + 15, dy1, GRY3)
rect(96, dy0, 96, dy1, BLK)
rect(111, dy0, 111, dy1, GRY2)
rect(159, dy0, 159, dy1, BLK)
rect(144, dy0, 144, dy1, GRY2)
# planks
rect(dx0, dy0, dx1, dy1, BRN2)
for px in range(dx0, dx1 + 1, 8):
    rect(px, dy0, px, dy1, BRN1)
    rect(px + 1, dy0, px + 1, dy1, BRN3)
    for gy in range(dy0 + 4, dy1, 10):
        d.point((px + 4, gy + (px // 8) % 3), fill=BRN1)
        d.point((px + 5, gy + (px // 8) % 3 + 1), fill=BRN1)
# bands
for by in (dy0 + 6, dy1 - 9):
    rect(dx0, by, dx1, by + 2, BRN1)
    for rx in range(dx0 + 4, dx1, 8):
        d.point((rx, by + 1), fill=BRN3)
# handle rings
rect(dx0 + 12, dy0 + 15, dx0 + 14, dy0 + 17, BRN3)
d.point((dx0 + 13, dy0 + 16), fill=BRN1)
rect(dx1 - 14, dy0 + 15, dx1 - 12, dy0 + 17, BRN3)
d.point((dx1 - 13, dy0 + 16), fill=BRN1)

# ---------- Torches ----------
for tx0 in (64, 176):
    cx = tx0 + 8
    # flame cell (palette: RED, ORG, YEL + backdrop)
    rect(tx0, 16, tx0 + 15, 31, GRY2)
    rect(cx - 1, 17, cx, 17, RED)
    rect(cx - 2, 18, cx + 1, 19, RED)
    rect(cx - 3, 20, cx + 2, 31, RED)
    rect(cx - 2, 21, cx + 1, 31, ORG)
    rect(cx - 1, 25, cx, 31, YEL)
    rect(cx - 3, 30, cx + 2, 31, RED)
    rect(cx - 2, 30, cx + 1, 31, ORG)
    rect(cx - 1, 30, cx, 31, YEL)
    # handle cell (palette: BRN1, BRN2, BRN3 + backdrop)
    rect(tx0, 32, tx0 + 15, 47, GRY2)
    rect(cx - 3, 32, cx + 2, 35, BRN1)       # cup
    rect(cx - 3, 32, cx + 2, 32, BRN3)
    rect(cx - 1, 36, cx, 47, BRN2)           # handle
    rect(cx - 1, 36, cx - 1, 47, BRN3)
    rect(cx + 1, 36, cx + 1, 47, BRN1)
    rect(cx - 3, 42, cx + 2, 43, BRN1)       # bracket

img.save("art.png")
