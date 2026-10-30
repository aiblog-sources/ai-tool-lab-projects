import numpy as np
from PIL import Image

W, H = 256, 224
HORIZON = 140      # sky/ground reference line
SHORE = 152        # where the lake starts
SUN_X, SUN_Y, SUN_R = 178, 118, 17

rng = np.random.default_rng(42)  # fixed seed so output is reproducible

def hexc(h):
    return np.array([int(h[i:i + 2], 16) for i in (0, 2, 4)], dtype=np.uint8)

# 4x4 Bayer matrix for ordered dithering (retro-style color banding)
BAYER = np.array([[0, 8, 2, 10],
                  [12, 4, 14, 6],
                  [3, 11, 1, 9],
                  [15, 7, 13, 5]]) / 16.0

def dither_index(t):
    """Map float palette positions to integer indices using ordered dithering."""
    yy, xx = np.mgrid[0:t.shape[0], 0:t.shape[1]]
    thr = BAYER[yy % 4, xx % 4]
    lo = np.floor(t).astype(int)
    frac = t - lo
    return lo + (frac > thr).astype(int)

def ridge(base, amp, step):
    """Return the top row of a mountain layer for every column."""
    n = W // step + 2
    pts = rng.uniform(-1, 1, n)
    h = np.interp(np.arange(W), np.arange(n) * step, pts)
    noise = np.convolve(rng.normal(0, 1, W), np.ones(3) / 3, mode="same")
    top = base - amp * (0.5 + 0.5 * h) + noise * 2.5
    return np.clip(np.round(top), 0, H - 1).astype(int)

# Coordinate grids
yy, xx = np.mgrid[0:H, 0:W]

# ---------- Sky ----------
sky_pal = np.array([hexc(c) for c in
                    ["1a1033", "2e1a4f", "4a2565", "7a2f6e",
                     "b5426a", "e0625a", "f5945a", "ffc070"]])
n_sky = len(sky_pal)
sky_t = np.clip((yy / HORIZON) ** 1.4, 0, 1) * (n_sky - 1)
canvas = sky_pal[dither_index(sky_t)].copy()

# ---------- Sun glow (dithered halo) ----------
d = np.hypot(xx - SUN_X, yy - SUN_Y)
glow_mask = (d >= SUN_R) & (d < SUN_R + 28) & (yy < HORIZON)
glow_t = np.clip(6 + (1 - (d - SUN_R) / 28), 6, 7)
glow_col = sky_pal[dither_index(glow_t)]
canvas[glow_mask] = glow_col[glow_mask]

# ---------- Stars ----------
for _ in range(25):
    sx = rng.integers(0, W)
    sy = rng.integers(0, 60)
    if np.hypot(sx - SUN_X, sy - SUN_Y) > 60:
        canvas[sy, sx] = hexc("cdb8ff")

# ---------- Sun disc with setting "cut" lines ----------
sun_mask = d < SUN_R
gap = (yy >= 112) & ((yy // 2) % 2 == 1)   # retro horizontal slices
visible = sun_mask & ~gap
canvas[visible & (yy <= SUN_Y + 4)] = hexc("fff0a0")
canvas[visible & (yy > SUN_Y + 4)] = hexc("ffc070")

# ---------- Far mountains (lit peaks) ----------
far_top = ridge(120, 40, 24)
far_mask = yy >= far_top[np.newaxis, :]
canvas[far_mask] = hexc("8a4a7a")
far_rim = far_mask & (yy < far_top[np.newaxis, :] + 2) & (far_top[np.newaxis, :] < 100)
canvas[far_rim] = hexc("ffb08a")

# ---------- Near mountains ----------
near_top = ridge(136, 26, 40)
near_mask = yy >= near_top[np.newaxis, :]
canvas[near_mask] = hexc("3d2a5c")
near_rim = near_mask & (yy < near_top[np.newaxis, :] + 1) & (near_top[np.newaxis, :] < 130)
canvas[near_rim] = hexc("6b3d6e")

# ---------- Lake ----------
water_pal = np.array([hexc(c) for c in
                      ["f5945a", "e0625a", "b5426a", "7a2f6e", "4a2565"]])
water_t = np.clip((yy - SHORE) / (H - SHORE), 0, 1) * (len(water_pal) - 1)
lake_mask = yy >= SHORE
canvas[lake_mask] = water_pal[dither_index(water_t)][lake_mask]

# Light ripple streaks
for _ in range(50):
    ry = rng.integers(SHORE + 4, H)
    rx = rng.integers(0, W - 16)
    rl = rng.integers(4, 14)
    canvas[ry, rx:rx + rl] = hexc("f5945a")

# Sun reflection: widens near shore, narrows toward the viewer, rippled
ripple_offset = rng.integers(-2, 3, size=H)
t_frac = np.clip((yy - SHORE) / (H - SHORE), 0, 1)
half_width = 2 + 18 * (1 - 0.8 * t_frac)
dx = np.abs(xx - SUN_X - ripple_offset[:, np.newaxis])
refl_mask = lake_mask & (dx < half_width) & (((yy - SHORE) % 3) != 2)
canvas[refl_mask] = hexc("fff0a0")

# ---------- Save ----------
Image.fromarray(canvas.astype(np.uint8), "RGB").save("art.png")
