from PIL import Image, ImageDraw
import random

W, H = 256, 192
# Master System palette: each channel is 0, 85, 170 or 255. 16 colours total.
PAL = [
    (0, 0, 0),        # 0 black
    (85, 85, 85),     # 1 dark gray
    (170, 170, 170),  # 2 light gray
    (0, 0, 85),       # 3 dark blue
    (85, 85, 170),    # 4 floor blue-gray
    (170, 170, 255),  # 5 floor light
    (85, 0, 0),       # 6 dark brown
    (170, 85, 0),     # 7 brown
    (255, 170, 0),    # 8 orange
    (255, 255, 0),    # 9 yellow
    (255, 85, 0),     # 10 flame red-orange
    (170, 0, 0),      # 11 red
    (255, 170, 85),   # 12 wood highlight
    (255, 255, 255),  # 13 white
    (0, 0, 0), (0, 0, 0),
]

img = Image.new("P", (W, H), 0)
flat = []
for c in PAL:
    flat += list(c)
img.putpalette(flat + [0] * (768 - len(flat)))
d = ImageDraw.Draw(img)
px = img.load()
rnd = random.Random(7)

FX0, FY0, FX1, FY1 = 16, 40, 240, 176  # floor area

# ---- Stone wall bricks over the whole screen ----
for row in range(H // 8):
    y = row * 8
    off = 8 if row % 2 else 0
    d.rectangle([0, y, W - 1, y + 7], fill=1)
    d.line([0, y, W - 1, y], fill=2)          # highlight on top
    d.line([0, y + 7, W - 1, y + 7], fill=0)  # mortar
    for x in range(-16 + off, W, 16):
        d.line([x + 15, y, x + 15, y + 7], fill=0)
        d.point((x + 16, y + 1), fill=2)
    for _ in range(6):
        sx, sy = rnd.randrange(W), y + rnd.randrange(1, 7)
        if px[sx, sy] == 1:
            px[sx, sy] = rnd.choice([0, 2])

# ---- Floor flagstones ----
d.rectangle([FX0, FY0, FX1 - 1, FY1 - 1], fill=4)
for ty in range(FY0, FY1, 16):
    for tx in range(FX0, FX1, 16):
        d.line([tx, ty, tx + 15, ty], fill=3)
        d.line([tx, ty, tx, ty + 15], fill=3)
        d.line([tx + 1, ty + 1, tx + 14, ty + 1], fill=5)
        d.line([tx + 1, ty + 1, tx + 1, ty + 14], fill=5)
        for _ in range(4):
            sx, sy = tx + rnd.randrange(3, 14), ty + rnd.randrange(3, 14)
            px[sx, sy] = rnd.choice([3, 5])
# closing edges of floor
d.line([FX0, FY1 - 1, FX1 - 1, FY1 - 1], fill=3)
d.line([FX1 - 1, FY0, FX1 - 1, FY1 - 1], fill=3)
# wall shadow on floor
d.rectangle([FX0, FY0, FX1 - 1, FY0 + 3], fill=3)
d.rectangle([FX0, FY0, FX0 + 2, FY1 - 1], fill=3)
for x in range(FX0, FX1):
    if (x % 2) == 0:
        px[x, FY0 + 4] = 3
for y in range(FY0, FY1):
    if (y % 2) == 0:
        px[FX0 + 3, y] = 3
# wall base edge
d.line([0, FY0 - 1, W - 1, FY0 - 1], fill=0)

# ---- Torch glow (dithered) ----
TORCHES = [72, 184]
TCY = 18
for tx in TORCHES:
    for y in range(0, 72):
        for x in range(tx - 34, tx + 35):
            if not (0 <= x < W):
                continue
            dist = ((x - tx) ** 2 + ((y - TCY) * 1.2) ** 2) ** 0.5
            if dist < 30 and (x + y) % 2 == 0:
                if px[x, y] == 1:
                    px[x, y] = 2
                elif px[x, y] == 4:
                    px[x, y] = 5
                elif px[x, y] == 3 and dist < 20:
                    px[x, y] = 4
            elif dist < 14 and (x + y) % 2 == 1:
                if px[x, y] == 1:
                    px[x, y] = 2

# ---- Door ----
dx0, dx1 = 112, 143
d.rectangle([dx0 - 5, 4, dx1 + 5, FY0 - 1], fill=0)          # frame outline
d.rectangle([dx0 - 4, 5, dx1 + 4, FY0 - 1], fill=2)          # stone frame
for y in range(5, FY0, 4):
    d.line([dx0 - 4, y, dx1 + 4, y], fill=1)
d.rectangle([dx0 - 1, 8, dx1 + 1, FY0 - 1], fill=0)          # dark opening
for i in range(4):
    x = dx0 + i * 8
    d.rectangle([x, 9, x + 7, FY0 - 1], fill=7)
    d.line([x, 9, x, FY0 - 1], fill=6)
    d.line([x + 1, 9, x + 1, FY0 - 1], fill=12)
    d.line([x + 4, 12, x + 4, FY0 - 2], fill=6) if i % 2 == 0 else None
# arched top corners
for (cx, cy) in [(dx0 - 1, 8), (dx1 + 1, 8)]:
    px[cx, cy] = 2
d.line([dx0, 9, dx1, 9], fill=6)
# iron bands
for by in (15, 30):
    d.rectangle([dx0, by, dx1, by + 2], fill=1)
    d.line([dx0, by, dx1, by], fill=2)
    d.line([dx0, by + 2, dx1, by + 2], fill=0)
    for rx in range(dx0 + 2, dx1, 8):
        px[rx, by + 1] = 13
# handle ring
d.rectangle([dx1 - 6, 22, dx1 - 3, 25], fill=9)
d.rectangle([dx1 - 5, 23, dx1 - 4, 24], fill=8)
# threshold
d.rectangle([dx0 - 5, FY0, dx1 + 5, FY0 + 2], fill=2)
d.line([dx0 - 5, FY0 + 3, dx1 + 5, FY0 + 3], fill=0)

# ---- Torches ----
for tx in TORCHES:
    cy = TCY
    # wall plate/shadow
    d.rectangle([tx - 3, cy + 6, tx + 3, cy + 22], fill=0)
    # pole
    d.rectangle([tx - 2, cy + 6, tx + 2, cy + 22], fill=6)
    d.line([tx - 1, cy + 6, tx - 1, cy + 22], fill=7)
    # cup
    d.polygon([(tx - 5, cy + 2), (tx + 5, cy + 2), (tx + 3, cy + 7), (tx - 3, cy + 7)], fill=1)
    d.line([tx - 5, cy + 2, tx + 5, cy + 2], fill=2)
    d.line([tx - 3, cy + 7, tx + 3, cy + 7], fill=0)
    # bracket
    d.rectangle([tx - 4, cy + 14, tx + 4, cy + 15], fill=1)
    # flame
    d.polygon([(tx, cy - 12), (tx + 2, cy - 6), (tx + 5, cy - 1), (tx + 4, cy + 2),
               (tx - 4, cy + 2), (tx - 5, cy - 1), (tx - 2, cy - 6)], fill=10)
    d.polygon([(tx, cy - 8), (tx + 3, cy - 2), (tx + 3, cy + 1),
               (tx - 3, cy + 1), (tx - 3, cy - 2)], fill=8)
    d.polygon([(tx, cy - 4), (tx + 1, cy - 1), (tx + 1, cy + 1),
               (tx - 1, cy + 1), (tx - 1, cy - 1)], fill=9)
    px[tx, cy - 12] = 11

# ---- Floor decoration: cracks and a small rubble ----
for (cx, cy) in [(60, 110), (180, 130), (130, 150)]:
    pts = [(cx, cy)]
    x, y = cx, cy
    for _ in range(8):
        x += rnd.choice([-2, -1, 1, 2])
        y += rnd.choice([0, 1, 2])
        pts.append((x, y))
    d.line(pts, fill=3)
for (rx, ry) in [(30, 160), (222, 58), (200, 160)]:
    d.rectangle([rx, ry, rx + 3, ry + 2], fill=1)
    d.line([rx, ry, rx + 3, ry], fill=2)
    d.line([rx, ry + 3, rx + 4, ry + 3], fill=3)

img.convert("RGB").save("art.png")
