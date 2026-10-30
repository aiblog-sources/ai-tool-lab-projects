from PIL import Image, ImageDraw
import numpy as np
import random

# Mega Drive: 320x224, 8x8 tiles, 3-bit RGB per channel (8 levels per channel).
W, H = 320, 224
T = 16                      # 16x16 metatiles drawn from 8x8 pixel blocks
COLS, ROWS = W // T, H // T  # 20 x 14 grid

rng = random.Random(7)      # fixed seed so the output is reproducible


def q(r, g, b):
    """Snap an RGB colour to the Mega Drive 3-bit-per-channel palette."""
    f = lambda v: min(7, round(v / 255 * 7)) * 36
    return (f(r), f(g), f(b))


# ---- palette --------------------------------------------------------------
FLOOR = [q(96, 92, 104), q(112, 106, 118), q(84, 80, 92)]
FLOOR_HI = q(140, 134, 146)
FLOOR_DARK = q(60, 56, 68)
FLOOR_MORTAR = q(40, 38, 48)

WALL = [q(120, 96, 80), q(104, 84, 72), q(136, 110, 92)]
WALL_HI = q(156, 132, 112)
WALL_MORTAR = q(44, 36, 32)

WOOD = [q(128, 72, 36), q(112, 60, 30)]
WOOD_DARK = q(60, 32, 16)
IRON = q(96, 100, 112)
GOLD = q(232, 184, 64)

BRACKET = q(70, 70, 80)
BOWL = q(40, 40, 48)
FLAME_OUT = q(230, 80, 20)
FLAME_MID = q(252, 170, 40)
FLAME_IN = q(252, 240, 120)

# ---- layout ---------------------------------------------------------------
# Walls: 2 tiles thick on all sides. Floor: cols 2..17, rows 2..11.
DOOR_COL = 9               # door spans cols 9-10 in the top wall
TORCH_TILES = [(6, 1), (13, 1)]  # (col, row) of wall sconces

img = Image.new("RGB", (W, H), FLOOR_MORTAR)
draw = ImageDraw.Draw(img)


def is_floor(col, row):
    return 2 <= col <= COLS - 3 and 2 <= row <= ROWS - 3


def draw_wall_tile(x, y):
    draw.rectangle([x, y, x + 15, y + 15], fill=WALL_MORTAR)
    for row in range(4):
        by = y + row * 4
        off = 0 if row % 2 == 0 else 4
        bx = x - 8 + off
        while bx < x + 16:
            x0 = max(bx, x)
            x1 = min(bx + 6, x + 15)
            if x1 >= x0:
                draw.rectangle([x0, by, x1, by + 2], fill=rng.choice(WALL))
                draw.line([x0, by, x1, by], fill=WALL_HI)
            bx += 8


def draw_floor_tile(x, y):
    draw.rectangle([x, y, x + 15, y + 15], fill=FLOOR_MORTAR)
    for sy in (0, 8):
        for sx in (0, 8):
            sx0, sy0 = x + sx, y + sy
            draw.rectangle([sx0, sy0, sx0 + 6, sy0 + 6], fill=rng.choice(FLOOR))
            draw.line([sx0, sy0, sx0 + 6, sy0], fill=FLOOR_HI)
            for _ in range(4):
                draw.point((sx0 + rng.randint(0, 6), sy0 + rng.randint(1, 6)),
                           fill=FLOOR_DARK)


# Walls and floor
for r in range(ROWS):
    for c in range(COLS):
        x, y = c * T, r * T
        if is_floor(c, r):
            draw_floor_tile(x, y)
        else:
            draw_wall_tile(x, y)

# Inner shadow cast by the walls onto the floor
fx0, fy0 = 2 * T, 2 * T
fx1, fy1 = (COLS - 2) * T - 1, (ROWS - 2) * T - 1
draw.line([fx0, fy0, fx1, fy0], fill=FLOOR_MORTAR)
draw.line([fx0, fy0 + 1, fx1, fy0 + 1], fill=FLOOR_DARK)
draw.line([fx0, fy1, fx1, fy1], fill=FLOOR_MORTAR)
draw.line([fx0, fy1 - 1, fx1, fy1 - 1], fill=FLOOR_DARK)
draw.line([fx0, fy0, fx0, fy1], fill=FLOOR_DARK)
draw.line([fx1, fy0, fx1, fy1], fill=FLOOR_DARK)

# Wooden door set into the top wall (2 tiles wide, 2 tiles tall)
dx, dy = DOOR_COL * T, 0
dw, dh = 2 * T, 2 * T
draw.rectangle([dx, dy, dx + dw - 1, dy + dh - 1], fill=WOOD[0])
for px in range(dx, dx + dw, 8):
    draw.rectangle([px + 1, dy + 1, px + 6, dy + dh - 3],
                   fill=WOOD[(px - dx) // 8 % 2])
    draw.line([px, dy, px, dy + dh - 1], fill=WOOD_DARK)
draw.line([dx + dw - 1, dy, dx + dw - 1, dy + dh - 1], fill=WOOD_DARK)
# Iron bands
draw.rectangle([dx + 1, dy + 5, dx + dw - 2, dy + 7], fill=IRON)
draw.rectangle([dx + 1, dy + 22, dx + dw - 2, dy + 24], fill=IRON)
# Handle
draw.ellipse([dx + 21, dy + 14, dx + 25, dy + 18], fill=GOLD)
# Outline and threshold
draw.rectangle([dx, dy, dx + dw - 1, dy + dh - 1], outline=WOOD_DARK)
draw.rectangle([dx, dy + dh - 2, dx + dw - 1, dy + dh - 1], fill=WOOD_DARK)

# Torch brackets and bowls (flames are added after the glow pass)
torch_centers = []
for col, row in TORCH_TILES:
    x, y = col * T, row * T
    cx, cy = x + 8, y + 12
    torch_centers.append((cx, cy))
    draw.rectangle([cx - 2, y + 8, cx + 1, y + 14], fill=BRACKET)
    draw.ellipse([cx - 3, cy - 3, cx + 3, cy + 3], fill=BOWL)

# Warm light pooling around the torches (applied to the whole image)
arr = np.array(img).astype(float)
yy, xx = np.mgrid[0:H, 0:W]
for cx, cy in torch_centers:
    d = np.sqrt((xx - cx) ** 2 + (yy - (cy - 4)) ** 2)
    falloff = np.clip(1.0 - d / 80.0, 0, 1) ** 2
    arr += falloff[..., None] * np.array([48.0, 28.0, 0.0])
arr = np.clip(arr, 0, 255)

# Snap to the Mega Drive palette levels
arr = np.clip(np.round(arr / 36.0), 0, 7) * 36
out = Image.fromarray(arr.astype(np.uint8), "RGB")
out_draw = ImageDraw.Draw(out)

# Flames on top of the glow
for cx, cy in torch_centers:
    out_draw.ellipse([cx - 3, cy - 9, cx + 3, cy - 2], fill=FLAME_OUT)
    out_draw.ellipse([cx - 2, cy - 7, cx + 2, cy - 3], fill=FLAME_MID)
    out_draw.ellipse([cx - 1, cy - 6, cx + 1, cy - 4], fill=FLAME_IN)

out.save("art.png")
