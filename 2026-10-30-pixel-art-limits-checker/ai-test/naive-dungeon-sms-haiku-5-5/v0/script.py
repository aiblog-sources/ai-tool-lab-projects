from PIL import Image, ImageDraw
import random

W, H = 256, 192
OUT = "art.png"

# Colors chosen to sit close to Sega Master System palette values
FLOOR_A = (120, 112, 104)
FLOOR_B = (104, 96, 88)
FLOOR_HI = (144, 136, 128)
FLOOR_SPECK = (84, 78, 72)
MORTAR = (56, 52, 50)
WALL_A = (72, 72, 96)
WALL_B = (88, 88, 112)
WALL_C = (64, 64, 84)
WALL_MORT = (40, 40, 56)
EDGE = (24, 24, 36)
WOOD = (136, 80, 40)
WOOD_L = (160, 104, 56)
WOOD_D = (80, 44, 20)
IRON = (168, 168, 176)
IRON_D = (72, 72, 80)
BRACKET = (96, 60, 30)
FLAME_R = (230, 60, 0)
FLAME = (255, 170, 0)
FLAME_Y = (255, 245, 110)
GLOW = (255, 150, 60)

# Floor interior (walls surround it: top 24px, other sides 16px)
FX0, FY0, FX1, FY1 = 16, 24, 239, 175
DX0, DX1 = 112, 143          # door opening in top wall
TORCHES = [(72, 8), (184, 8)]  # flame centres in top wall

img = Image.new("RGB", (W, H), WALL_A)
px = img.load()
d = ImageDraw.Draw(img)

# 1. Walls: running-bond brick pattern over the whole screen
for y in range(H):
    row = y // 8
    off = 8 if row % 2 else 0
    for x in range(W):
        if y % 8 == 7 or (x + off) % 16 == 15:
            px[x, y] = WALL_MORT
        else:
            col = (x + off) // 16
            v = (col * 37 + row * 17 + col * row) % 3
            px[x, y] = (WALL_A, WALL_B, WALL_C)[v]

# 2. Flagstone floor: 16x16 tiles with bevel and speckle
rng = random.Random(7)
for ty in range(FY0, FY1 + 1, 16):
    for tx in range(FX0, FX1 + 1, 16):
        shade = (tx // 16 + (ty // 16) * 3) % 2
        base = FLOOR_A if shade == 0 else FLOOR_B
        x1 = min(tx + 15, FX1)
        y1 = min(ty + 15, FY1)
        d.rectangle([tx, ty, x1, y1], fill=base)
        for _ in range(6):
            sx = rng.randint(tx + 1, x1 - 1)
            sy = rng.randint(ty + 1, y1 - 1)
            px[sx, sy] = FLOOR_SPECK
        d.line([tx, ty, x1, ty], fill=FLOOR_HI)     # top bevel
        d.line([tx, ty, tx, y1], fill=FLOOR_HI)     # left bevel
        d.line([tx, y1, x1, y1], fill=MORTAR)       # bottom joint
        d.line([x1, ty, x1, y1], fill=MORTAR)       # right joint

# Shadow where walls meet the floor
d.line([FX0 - 1, FY0 - 1, FX1 + 1, FY0 - 1], fill=EDGE)
d.line([FX0 - 1, FY0, FX0 - 1, FY1], fill=EDGE)
d.line([FX1 + 1, FY0, FX1 + 1, FY1], fill=EDGE)
d.line([FX0 - 1, FY1 + 1, FX1 + 1, FY1 + 1], fill=EDGE)

# 3. Wooden door in the top wall
d.rectangle([DX0 - 2, 0, DX1 + 2, 23], fill=EDGE)  # frame
for x in range(DX0, DX1 + 1):
    plank = (x - DX0) // 8
    d.line([x, 2, x, 23], fill=WOOD if plank % 2 == 0 else WOOD_L)
    if (x - DX0) % 8 == 0:
        d.line([x, 2, x, 23], fill=WOOD_D)           # plank seam
d.rectangle([DX0, 6, DX1, 7], fill=IRON_D)           # iron bands
d.rectangle([DX0, 18, DX1, 19], fill=IRON_D)
d.ellipse([136, 11, 139, 14], fill=IRON)             # door knob
d.point((137, 12), fill=WOOD_D)

# 4. Warm torchlight glow on the floor
R = 44.0
alpha = {}
for tx, ty in TORCHES:
    for y in range(FY0, FY1 + 1):
        for x in range(FX0, FX1 + 1):
            dist = ((x - tx) ** 2 + (y - ty) ** 2) ** 0.5
            if dist < R:
                a = 0.5 * (1.0 - dist / R) ** 2
                alpha[(x, y)] = alpha.get((x, y), 0.0) + a
for (x, y), a in alpha.items():
    a = min(a, 0.7)
    r, g, b = px[x, y]
    px[x, y] = (int(r + (GLOW[0] - r) * a),
                int(g + (GLOW[1] - g) * a),
                int(b + (GLOW[2] - b) * a))

# 5. Wall-mounted torches (drawn last so flames stay bright)
def draw_torch(cx, cy):
    d.rectangle([cx - 1, cy + 4, cx + 1, cy + 8], fill=BRACKET)  # handle
    d.rectangle([cx - 3, cy + 2, cx + 3, cy + 4], fill=IRON_D)   # cup
    d.ellipse([cx - 4, cy - 5, cx + 4, cy + 3], fill=FLAME_R)    # outer flame
    d.ellipse([cx - 3, cy - 3, cx + 3, cy + 1], fill=FLAME)      # mid flame
    d.ellipse([cx - 1, cy - 1, cx + 1, cy + 1], fill=FLAME_Y)    # core

for tx, ty in TORCHES:
    draw_torch(tx, ty)

img.save(OUT)
