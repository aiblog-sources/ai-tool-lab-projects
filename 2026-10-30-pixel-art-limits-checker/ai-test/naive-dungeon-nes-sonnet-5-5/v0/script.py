from PIL import Image, ImageDraw
import random

W, H = 256, 240
rng = random.Random(7)

# Limited NES-style palette
BLK   = (0, 0, 0)
DK    = (44, 44, 48)
GRY1  = (84, 84, 84)
GRY2  = (124, 124, 124)
GRY3  = (170, 170, 170)
WHT   = (236, 238, 236)
BRN1  = (72, 36, 8)
BRN2  = (136, 80, 16)
BRN3  = (188, 124, 40)
RED   = (200, 40, 24)
ORG   = (252, 152, 56)
YEL   = (252, 224, 120)
GLOW  = (196, 140, 72)

img = Image.new("RGB", (W, H), BLK)
d = ImageDraw.Draw(img)

def rect(x0, y0, x1, y1, c):
    d.rectangle([x0, y0, x1, y1], fill=c)

TOP, BOT, SIDE = 40, 16, 16
fx0, fy0, fx1, fy1 = SIDE, TOP, W - SIDE - 1, H - BOT - 1

# ---------- Walls (staggered bricks) ----------
rect(0, 0, W - 1, H - 1, DK)
for row in range(0, H // 8):
    y = row * 8
    off = 8 if row % 2 else 0
    for bx in range(-16, W + 16, 16):
        x = bx + off
        x0, x1 = max(x, 0), min(x + 15, W - 1)
        if x1 < 0 or x0 > W - 1:
            continue
        rect(x0, y, x1, y + 7, BLK)                  # mortar
        rect(x + 1, y + 1, x + 15, y + 7, GRY2)      # brick face
        rect(x + 1, y + 1, x + 15, y + 1, GRY3)      # top highlight
        rect(x + 15, y + 2, x + 15, y + 7, GRY1)     # right shade
        rect(x + 1, y + 7, x + 15, y + 7, GRY1)      # bottom shade
        for _ in range(2):
            px, py = x + rng.randint(2, 13), y + rng.randint(2, 5)
            d.point((px, py), fill=rng.choice([GRY1, GRY3]))
# clip bricks to wall area by repainting floor next
# ---------- Floor (16x16 slabs) ----------
for sy in range(fy0, fy1 + 1, 16):
    for sx in range(fx0, fx1 + 1, 16):
        base = rng.choice([GRY1, GRY1, (92, 92, 92)])
        rect(sx, sy, sx + 15, sy + 15, DK)
        rect(sx + 1, sy + 1, sx + 15, sy + 15, base)
        rect(sx + 1, sy + 1, sx + 14, sy + 1, GRY2)
        rect(sx + 1, sy + 1, sx + 1, sy + 14, GRY2)
        rect(sx + 15, sy + 2, sx + 15, sy + 15, DK)
        rect(sx + 2, sy + 15, sx + 15, sy + 15, DK)
        for _ in range(4):
            d.point((sx + rng.randint(3, 13), sy + rng.randint(3, 13)),
                    fill=rng.choice([DK, GRY2]))
        if rng.random() < 0.25:  # crack
            cx, cy = sx + rng.randint(4, 10), sy + rng.randint(4, 10)
            for i in range(rng.randint(3, 5)):
                d.point((cx + i, cy + (i // 2)), fill=DK)

# Wall shadow cast onto floor
for i, row in enumerate(range(fy0 + 1, fy0 + 5)):
    for x in range(fx0, fx1 + 1):
        if i < 2 or (x + row) % 2 == 0:
            d.point((x, row), fill=DK if i < 3 else GRY1)
for y in range(fy0, fy1 + 1):
    for x in (fx0 + 1, fx0 + 2, fx1 - 2, fx1 - 1):
        if (x + y) % 2 == 0:
            d.point((x, y), fill=DK)
# inner wall lip highlight
rect(fx0 - 2, fy0 - 2, fx1 + 2, fy0 - 2, GRY3) if False else None

# ---------- Torch glow (dithered) ----------
torches = [(68, 20), (188, 20)]
for tx, ty in torches:
    for y in range(0, TOP):
        for x in range(tx - 18, tx + 19):
            if (x + y) % 2 == 0 and (x - tx) ** 2 + ((y - ty) * 1.2) ** 2 < 17 ** 2:
                if img.getpixel((x, y)) in (GRY2, GRY3, GRY1):
                    d.point((x, y), fill=GLOW)

# ---------- Door ----------
dx0, dx1 = 112, 143
dy0 = 10
# stone frame
rect(dx0 - 6, dy0 - 6, dx1 + 6, TOP - 1, BLK)
rect(dx0 - 5, dy0 - 5, dx1 + 5, TOP - 1, GRY3)
rect(dx0 - 5, dy0 - 5, dx1 + 5, dy0 - 5, WHT)
rect(dx0 - 5, dy0 - 4, dx0 - 4, TOP - 1, GRY2)
rect(dx0 - 3, dy0 - 3, dx1 + 3, TOP - 1, BLK)
# planks
rect(dx0, dy0, dx1, TOP - 1, BRN2)
for px in range(dx0, dx1 + 1, 8):
    rect(px, dy0, px, TOP - 1, BRN1)
    rect(px + 1, dy0, px + 1, TOP - 1, BRN3)
    for gy in range(dy0 + 3, TOP, 7):
        d.point((px + 4, gy + (px // 8) % 3), fill=BRN1)
        d.point((px + 5, gy + (px // 8) % 3 + 1), fill=BRN1)
# arched top corners
for (cx, cy) in [(dx0, dy0), (dx1, dy0)]:
    d.point((cx, cy), fill=BLK)
# iron bands
for by in (dy0 + 6, TOP - 9):
    rect(dx0, by, dx1, by + 3, DK)
    rect(dx0, by, dx1, by, GRY1)
    for rx in range(dx0 + 3, dx1, 8):
        d.point((rx, by + 2), fill=GRY3)
# handle rings
rect(dx0 + 12, dy0 + 15, dx0 + 14, dy0 + 17, GRY3)
rect(dx0 + 13, dy0 + 16, dx0 + 13, dy0 + 16, DK)
rect(dx1 - 14, dy0 + 15, dx1 - 12, dy0 + 17, GRY3)
rect(dx1 - 13, dy0 + 16, dx1 - 13, dy0 + 16, DK)
# threshold
rect(dx0 - 5, TOP, dx1 + 5, TOP + 1, GRY3)
rect(dx0 - 5, TOP + 2, dx1 + 5, TOP + 2, DK)

# ---------- Torches ----------
for tx, ty in torches:
    # wall plate
    rect(tx - 3, ty + 6, tx + 3, ty + 7, BLK)
    # handle
    rect(tx - 1, ty + 2, tx + 1, ty + 16, BRN2)
    rect(tx - 1, ty + 2, tx - 1, ty + 16, BRN3)
    rect(tx + 1, ty + 2, tx + 1, ty + 16, BRN1)
    # iron cup
    rect(tx - 3, ty, tx + 3, ty + 3, DK)
    rect(tx - 3, ty, tx + 3, ty, GRY2)
    # flame
    flame = [
        (-1, 1, -12, -11, RED),
        (-2, 2, -10, -8, RED),
        (-3, 3, -8, -2, RED),
        (-2, 2, -6, -1, ORG),
        (-1, 1, -4, -1, YEL),
        (0, 0, -9, -7, ORG),
    ]
    for a, b, ya, yb, c in flame:
        rect(tx + a, ty + ya, tx + b, ty + yb, c)
    rect(tx - 3, ty - 2, tx + 3, ty - 1, RED)
    rect(tx - 2, ty - 2, tx + 2, ty - 1, ORG)
    rect(tx - 1, ty - 2, tx + 1, ty - 1, YEL)
    # shadow of torch on wall
    rect(tx + 4, ty + 6, tx + 4, ty + 16, DK)

# outer border darkening on walls
rect(0, 0, W - 1, 0, BLK)
rect(0, H - 1, W - 1, H - 1, BLK)
rect(0, 0, 0, H - 1, BLK)
rect(W - 1, 0, W - 1, H - 1, BLK)

img.save("art.png")
