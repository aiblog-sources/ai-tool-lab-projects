import numpy as np
from PIL import Image

# Mega Drive screen size and horizon line
W, H = 320, 224
HOR = 150

# One 16-colour palette line. Each channel is a 3-bit MD level (0..7),
# matching the hardware's 512-colour (BGR333) space.
PAL = [
    (1, 0, 2), (2, 0, 3), (4, 0, 4), (5, 1, 3),   # 0-3  sky (top, dark violet)
    (6, 2, 3), (7, 3, 2), (7, 5, 1), (7, 7, 3),   # 4-7  sky (toward horizon, orange/yellow)
    (0, 0, 1), (1, 0, 1), (2, 1, 2),              # 8-10 mountains (near, mid, far)
    (2, 1, 3), (3, 2, 4), (5, 3, 5), (7, 5, 4),   # 11-14 lake (deep -> shore)
    (7, 7, 7),                                    # 15 sun core / highlight
]
assert len(PAL) == 16
for c in PAL:
    assert all(0 <= v <= 7 for v in c)

# Sun position and radius
SX, SY, SR = 236, 114, 19

idx = np.zeros((H, W), dtype=np.uint8)
yy, xx = np.mgrid[0:H, 0:W]

# Sky: flat bands, dark at top, bright at horizon
for y in range(HOR):
    idx[y, :] = min(7, int(y * 8 / HOR))

# Sun glow and core
d2 = (xx - SX) ** 2 + (yy - SY) ** 2
idx[d2 <= (SR + 7) ** 2] = 7
idx[d2 <= SR * SR] = 15

# Mountain ridge profiles (top row of each layer, per column)
x = np.arange(W, dtype=float)
far = 112 + 14 * np.sin(x / 37 + 0.6) + 7 * np.sin(x / 13 + 2.0)
mid = 124 + 10 * np.sin(x / 29 + 3.1) + 4 * np.sin(x / 11)
near = 138 + 6 * np.sin(x / 23 + 1.0) + 3 * np.sin(x / 7 + 0.4)
far, mid, near = [np.clip(np.round(a), 0, HOR - 1).astype(int) for a in (far, mid, near)]

# Fill far -> mid -> near so nearer layers overwrite farther ones
for top, color in ((far, 10), (mid, 9), (near, 8)):
    for xi in range(W):
        idx[top[xi]:HOR, xi] = color

# Lake: below the horizon, with mirrored mountains, sun reflection and ripples
for y in range(HOR, H):
    base = 14 - min(3, (y - HOR) * 4 // (H - HOR))   # brighter near shore
    off = int(round(3 * np.sin(y * 0.9)))            # horizontal ripple offset
    yr = 2 * HOR - 1 - y                             # mirrored row across the water
    for xi in range(W):
        xr = min(W - 1, max(0, xi + off))
        if (xr - SX) ** 2 + (yr - SY) ** 2 <= SR * SR:
            idx[y, xi] = 15 if y % 3 else base       # broken sun-glint streaks
        elif yr >= near[xr]:
            idx[y, xi] = 8
        elif yr >= mid[xr]:
            idx[y, xi] = 9
        elif yr >= far[xr]:
            idx[y, xi] = 10
        else:
            idx[y, xi] = base

# Check the tile budget: 8x8 cells, unique tiles (the plane has 40x28 cells)
tiles = idx.reshape(H // 8, 8, W // 8, 8).transpose(0, 2, 1, 3).reshape(-1, 64)
unique_tiles = np.unique(tiles, axis=0).shape[0]
print(f"Colours used: {len(np.unique(idx))} / 16 (one palette line)")
print(f"Unique 8x8 tiles: {unique_tiles} (of {tiles.shape[0]} cells)")

# Convert MD levels to 8-bit RGB for the PNG
pal_rgb = np.array([[c[0] * 255 // 7, c[1] * 255 // 7, c[2] * 255 // 7] for c in PAL], dtype=np.uint8)
out = pal_rgb[idx]
Image.fromarray(out, "RGB").save("art.png")
print("Saved art.png (320x224)")
