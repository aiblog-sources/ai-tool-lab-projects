import numpy as np
from PIL import Image

# SNES background, one layer, 4bpp (16-colour palette), 256x224 full screen.
# Colours are stored as 5-bit RGB (SNES CGRAM) and expanded to 8-bit for the PNG,
# so every colour in the file maps back to an exact SNES colour.
W, H = 256, 224
HORIZON = 140

PAL5 = [
    (2, 3, 10),    # 0  deep night (also the backdrop/transparent colour)
    (4, 5, 16),    # 1
    (8, 6, 24),    # 2
    (14, 7, 30),   # 3
    (20, 9, 28),   # 4
    (26, 12, 22),  # 5
    (30, 16, 16),  # 6
    (31, 22, 12),  # 7
    (31, 28, 14),  # 8
    (31, 31, 22),  # 9  sun glow
    (14, 9, 22),   # 10 far mountains
    (6, 4, 12),    # 11 near mountains
    (2, 10, 18),   # 12 lake dark
    (6, 18, 26),   # 13 lake light
    (30, 18, 8),   # 14 sun reflection
    (31, 31, 26),  # 15 sun core
]
assert len(PAL5) == 16
assert all(0 <= c <= 31 for col in PAL5 for c in col)


def expand5(v):
    # 5-bit -> 8-bit with bit replication (matches how the SNES drives the DAC)
    return (v << 3) | (v >> 2)


RGB = np.array([[expand5(c) for c in col] for col in PAL5], dtype=np.uint8)

BAYER = np.array([[0, 8, 2, 10],
                  [12, 4, 14, 6],
                  [3, 11, 1, 9],
                  [15, 7, 13, 5]]) / 16.0

rng = np.random.default_rng(7)
yy, xx = np.mgrid[0:H, 0:W]
x = np.arange(W)
thr = BAYER[yy % 4, xx % 4]
idx = np.zeros((H, W), dtype=np.uint8)

# --- Sky: dithered gradient from deep night (top) to warm light (horizon) ---
sky = yy < HORIZON
p = np.clip((yy / HORIZON) ** 2.2, 0, 1) * 9.0
base = np.floor(p).astype(int)
frac = p - base
sky_idx = np.clip(base + (frac > thr), 0, 9)
idx[sky] = sky_idx[sky]

# --- Sun: solid core with dithered glow ring ---
cx, cy = 128, 124
dist = np.hypot(xx - cx, yy - cy)
glow_strength = np.clip(1 - (dist - 18) / 12.0, 0, 1)
glow = sky & (dist > 18) & (dist <= 30) & (thr < glow_strength)
idx[glow] = 9
core = sky & (dist <= 18)
idx[core] = 15

# --- Far mountains (hazy purple) ---
noise_far = np.convolve(rng.normal(0, 2.0, W), np.ones(5) / 5, mode="same")
far_y = (96 + 13 * np.sin(x * 0.035 + 1.0)
         + 8 * np.sin(x * 0.09 + 2.3)
         + 4 * np.sin(x * 0.21 + 0.5)
         + noise_far).astype(int)
idx[yy >= far_y[None, :]] = 10

# --- Near mountains (dark silhouette) ---
noise_near = np.convolve(rng.normal(0, 2.0, W), np.ones(5) / 5, mode="same")
near_y = (122 + 10 * np.sin(x * 0.027 + 4.0)
          + 6 * np.sin(x * 0.11 + 0.7)
          + 3 * np.sin(x * 0.37)
          + noise_near).astype(int)
idx[yy >= near_y[None, :]] = 11

# --- Lake (below the horizon) ---
lake = yy > HORIZON
d = yy - (HORIZON + 1)
lake_base = np.where(d < 18, 13, 12)
ripple = (d % 4 == 1) & (rng.random((H, W)) < 0.35)
lake_base = np.where(ripple, 13, lake_base)
idx[lake] = lake_base[lake]

# Sun reflection on the water: narrows with distance from the shore
half_width = np.maximum(3.0, 20.0 - d * 0.12)
refl = lake & (np.abs(xx - cx) < half_width)
idx[refl & (d % 3 != 0)] = 14
idx[refl & (d % 3 == 0)] = 9

# --- Output ---
img = Image.fromarray(idx, mode="P")
pal_bytes = RGB.flatten().tolist()
pal_bytes += [0] * (768 - len(pal_bytes))
img.putpalette(pal_bytes)
img.save("art.png")

# --- Hardware checks ---
assert img.size == (256, 224)
assert int(idx.max()) < 16
# 8x8 tiles: 32x28 tiles on screen; each tile must be unique-counted for VRAM
tiles = idx.reshape(28, 8, 32, 8).transpose(0, 2, 1, 3).reshape(-1, 64)
unique_tiles = np.unique(tiles, axis=0).shape[0]
assert unique_tiles <= 1024, f"too many unique tiles: {unique_tiles}"
print(f"Saved art.png: 256x224, 16 colours (4bpp), {unique_tiles} unique 8x8 tiles")
