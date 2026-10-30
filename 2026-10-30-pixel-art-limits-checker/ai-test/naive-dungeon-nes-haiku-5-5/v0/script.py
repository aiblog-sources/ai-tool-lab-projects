import numpy as np
from PIL import Image, ImageDraw

W, H = 256, 240
rng = np.random.default_rng(7)  # fixed seed so output is deterministic

# NES-style limited palette
WALL_A = (72, 64, 76)
WALL_B = (92, 84, 96)
WALL_HI = (124, 116, 128)
MORTAR = (40, 36, 46)
FLOOR_A = (88, 88, 98)
FLOOR_B = (104, 102, 112)
FLOOR_HI = (128, 126, 136)
SHADOW = (30, 26, 34)
WOOD = (136, 84, 40)
WOOD_L = (168, 112, 64)
WOOD_D = (80, 48, 24)
IRON = (160, 162, 170)
BRAZIER = (72, 48, 32)
RIM = (128, 76, 36)
COALS = (200, 80, 24)
FLAME_O = (248, 120, 24)
FLAME_Y = (252, 200, 48)
FLAME_W = (255, 248, 176)
GLOW_TINT = np.array([255, 170, 80], dtype=np.float32)

img = Image.new("RGB", (W, H), WALL_A)
d = ImageDraw.Draw(img)


def brick_wall(x0, y0, x1, y1):
    """Staggered 16x8 stone blocks with mortar lines."""
    for by in range(y0, y1, 8):
        row = (by - y0) // 8
        off = 8 if row % 2 else 0
        for bx in range(x0 - off, x1, 16):
            c = WALL_A if rng.random() < 0.5 else WALL_B
            left, right = max(bx, x0), min(bx + 15, x1 - 1)
            if left > right:
                continue
            d.rectangle([left, by, right, by + 7], fill=c)
            d.line([(left, by + 7), (right, by + 7)], fill=MORTAR)   # bottom mortar
            if bx + 15 <= x1 - 1 and bx >= x0:
                d.line([(bx + 15, by), (bx + 15, by + 7)], fill=MORTAR)  # right mortar
            d.line([(left, by), (right, by)], fill=WALL_HI)          # top highlight


# Walls cover the whole screen first; the floor is painted over the interior
brick_wall(0, 0, W, H)

# Flagstone floor inside the 2-tile wall border: x 16..239, y 16..223
for gy in range(16, 224, 16):
    for gx in range(16, 240, 16):
        c = FLOOR_A if rng.random() < 0.5 else FLOOR_B
        d.rectangle([gx, gy, gx + 15, gy + 15], fill=c)
        d.line([(gx + 15, gy), (gx + 15, gy + 15)], fill=MORTAR)
        d.line([(gx, gy + 15), (gx + 15, gy + 15)], fill=MORTAR)
        d.line([(gx, gy), (gx + 14, gy)], fill=FLOOR_HI)
        d.line([(gx, gy), (gx, gy + 14)], fill=FLOOR_HI)
        for _ in range(3):  # speckles for texture
            px = gx + int(rng.integers(1, 14))
            py = gy + int(rng.integers(1, 14))
            d.point((px, py), fill=FLOOR_HI if rng.random() < 0.5 else MORTAR)

# Wall-to-floor shadow along the inner edges (gives depth in top-down view)
d.rectangle([16, 16, 239, 17], fill=SHADOW)
d.rectangle([16, 222, 239, 223], fill=SHADOW)
d.rectangle([16, 16, 17, 223], fill=SHADOW)
d.rectangle([238, 16, 239, 223], fill=SHADOW)

# Warm torch light: blend floor toward a warm tint with quadratic falloff
torches = [(44, 52), (212, 52)]
arr = np.array(img).astype(np.float32)
yy, xx = np.mgrid[0:H, 0:W]
interior = (xx >= 16) & (xx < 240) & (yy >= 16) & (yy < 224)
for cx, cy in torches:
    dist = np.sqrt((xx - cx) ** 2 + (yy - cy) ** 2)
    f = np.clip(1.0 - dist / 46.0, 0.0, 1.0) ** 2 * 0.45
    f = f[..., None]
    arr = np.where(interior[..., None], arr * (1 - f) + GLOW_TINT * f, arr)
img = Image.fromarray(np.clip(arr, 0, 255).astype(np.uint8), "RGB")
d = ImageDraw.Draw(img)

# Wooden door centered in the top wall (x 120..135, y 0..15)
d.rectangle([118, 0, 137, 15], fill=WOOD_D)           # frame
for px in range(120, 136, 4):                         # vertical planks
    c = WOOD if (px // 4) % 2 == 0 else WOOD_L
    d.rectangle([px, 0, px + 3, 15], fill=c)
    d.line([(px + 3, 0), (px + 3, 15)], fill=WOOD_D)  # plank seams
d.rectangle([120, 4, 135, 4], fill=IRON)              # iron bands
d.rectangle([120, 11, 135, 11], fill=IRON)
d.rectangle([131, 7, 132, 8], fill=IRON)              # handle
d.rectangle([120, 15, 135, 16], fill=WOOD_D)          # threshold

# Standing torches seen from above: brazier, coals, flame
def torch(cx, cy):
    d.ellipse([cx - 6, cy - 4, cx + 7, cy + 7], fill=SHADOW)          # cast shadow
    d.ellipse([cx - 5, cy - 5, cx + 5, cy + 5], fill=BRAZIER, outline=MORTAR)
    d.ellipse([cx - 3, cy - 3, cx + 3, cy + 3], fill=RIM)
    d.ellipse([cx - 2, cy - 2, cx + 2, cy + 2], fill=COALS)
    d.ellipse([cx - 4, cy - 10, cx + 4, cy - 2], fill=FLAME_O)        # outer flame
    d.ellipse([cx - 2, cy - 8, cx + 2, cy - 3], fill=FLAME_Y)         # inner flame
    d.point((cx, cy - 6), fill=FLAME_W)                              # hot core


for cx, cy in torches:
    torch(cx, cy)

img.save("art.png")
