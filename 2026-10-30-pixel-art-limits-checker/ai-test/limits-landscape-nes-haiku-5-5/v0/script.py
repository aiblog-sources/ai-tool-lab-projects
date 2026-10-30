from collections import Counter

import numpy as np
from PIL import Image

W, H = 256, 240
HOR = 150  # horizon line (sky above, lake below)

# Approximate NES master palette (2C02 PPU), indices 0x00-0x3F
MASTER = {
    0x01: (0, 30, 116), 0x02: (8, 16, 144), 0x03: (48, 0, 136), 0x04: (68, 0, 100),
    0x05: (92, 0, 48), 0x06: (84, 4, 0), 0x07: (60, 24, 0), 0x08: (32, 42, 0),
    0x09: (8, 58, 0), 0x0A: (0, 64, 0), 0x0B: (0, 60, 0), 0x0C: (0, 50, 60),
    0x0D: (0, 0, 0),
    0x10: (152, 150, 152), 0x11: (8, 76, 196), 0x12: (48, 50, 236), 0x13: (92, 30, 228),
    0x14: (136, 20, 176), 0x15: (160, 20, 100), 0x16: (152, 34, 32), 0x17: (120, 60, 0),
    0x18: (84, 90, 0), 0x19: (40, 114, 0), 0x1A: (8, 124, 0), 0x1B: (0, 118, 40),
    0x1C: (0, 102, 120),
    0x20: (236, 238, 236), 0x21: (76, 154, 236), 0x22: (120, 124, 236), 0x23: (176, 98, 236),
    0x24: (228, 84, 236), 0x25: (236, 88, 180), 0x26: (236, 106, 100), 0x27: (212, 136, 32),
    0x28: (160, 170, 0), 0x29: (116, 196, 0), 0x2A: (76, 208, 32), 0x2B: (56, 204, 108),
    0x2C: (56, 180, 204), 0x2D: (60, 60, 60),
    0x30: (236, 238, 236), 0x31: (168, 204, 236), 0x32: (188, 188, 236), 0x33: (212, 178, 236),
    0x34: (236, 174, 236), 0x35: (236, 174, 212), 0x36: (236, 180, 176), 0x37: (228, 196, 144),
    0x38: (204, 210, 120), 0x39: (180, 222, 120), 0x3A: (168, 226, 144), 0x3B: (152, 226, 180),
    0x3C: (160, 214, 228), 0x3D: (160, 162, 160),
}

UNIVERSAL = 0x01  # shared color 0 of every background palette
# Four background palettes: [universal, color1, color2, color3]
PALETTES = [
    [UNIVERSAL, 0x12, 0x13, 0x23],  # blues / lilac (upper sky)
    [UNIVERSAL, 0x24, 0x26, 0x14],  # pinks / magenta (dusk glow, far peaks)
    [UNIVERSAL, 0x27, 0x37, 0x38],  # orange / peach / pale yellow (horizon, sun)
    [UNIVERSAL, 0x1C, 0x2C, 0x3C],  # teals / cyan (lake)
]
PAL_RGB = np.array([[MASTER[c] for c in p] for p in PALETTES], np.float32)  # (4,4,3)

# ---------------- Scene rendering (full-colour RGB) ----------------
ys, xs = np.mgrid[0:H, 0:W].astype(np.float32)
x = np.arange(W, dtype=np.float32)
img = np.zeros((H, W, 3), np.float32)

# Sky gradient: deep blue -> purple -> pink -> orange
sky_t = np.clip(ys / HOR, 0, 1)
stops_t = [0.0, 0.45, 0.8, 1.0]
stops_c = np.array([(18, 28, 100), (110, 50, 140), (230, 96, 110), (252, 180, 90)], np.float32)
for ch in range(3):
    img[..., ch] = np.interp(sky_t, stops_t, stops_c[:, ch])

# Sun glow and disc
d = np.hypot(xs - 128, ys - HOR)
glow = np.clip(1 - d / 70, 0, 1) ** 2
img += glow[..., None] * np.array([60, 30, 0], np.float32)
img[d < 22] = (255, 236, 150)


# Mountain ridges
def ridge(base, waves):
    return base + sum(a * np.sin(f * x + p) for a, f, p in waves)


far = ridge(128, [(12, 0.045, 3.0), (6, 0.17, 1.0)])
near = ridge(118, [(22, 0.03, 0.0), (12, 0.085, 1.0), (5, 0.23, 2.0)])

far_mask = (ys >= far[None, :]) & (ys < HOR)
img[far_mask] = (120, 50, 130)

near_mask = (ys >= near[None, :]) & (ys < HOR)
img[near_mask] = (40, 20, 70)

# Lake: vertical gradient below the horizon
lake = ys >= HOR
lt = np.clip((ys - HOR) / (H - HOR), 0, 1)[..., None]
lake_col = (1 - lt) * np.array([70, 160, 180], np.float32) + lt * np.array([20, 80, 130], np.float32)
img = np.where(lake[..., None], lake_col, img)

# Mirrored mountain reflection near the shore
src_y = 2 * HOR - ys
mirror = lake & (ys - HOR < 22) & (src_y >= near[None, :])
img[mirror] = (45, 22, 80)

# Ripples
ripple = lake & (np.sin(xs * 0.35 + ys * 1.7) > 0.97)
img[ripple] = (200, 235, 235)

# Sun reflection column on the water (drawn last)
sun_refl = lake & (np.abs(xs - 128) < (3 + 10 * (ys - HOR) / (H - HOR))) & ((ys.astype(int) // 3) % 2 == 0)
img[sun_refl] = (250, 230, 160)

# ---------------- NES quantization ----------------
# Pick the best palette for each 16x16 quadrant, then map pixels to it.
diff = img[:, :, None, None, :] - PAL_RGB[None, None]          # (H,W,4pal,4col,3)
dist = (diff ** 2).sum(-1)                                     # (H,W,4pal,4col)
pix_idx_all = dist.argmin(-1)                                  # (H,W,4pal)
pix_err_all = dist.min(-1)                                     # (H,W,4pal)

# Sum error per 16x16 quadrant for each palette
quad_err = pix_err_all.reshape(H // 16, 16, W // 16, 16, 4).sum(axis=(1, 3))  # (15,16,4)
quad_pal = quad_err.argmin(-1)                                 # (15,16)

pal_full = np.repeat(np.repeat(quad_pal, 16, 0), 16, 1)        # (240,256)
idx_full = np.take_along_axis(pix_idx_all, pal_full[..., None], axis=2)[..., 0]

# Split into 8x8 tiles (30 x 32 grid)
tiles = idx_full.astype(np.uint8).reshape(30, 8, 32, 8).transpose(0, 2, 1, 3).reshape(30, 32, 64)
tile_pal = np.repeat(np.repeat(quad_pal, 2, 0), 2, 1)          # per-tile palette for reference

# Enforce the 256-tile pattern-table limit
keys = [row.tobytes() for row in tiles.reshape(-1, 64)]
counts = Counter(keys)
uniq = sorted(counts, key=lambda k: -counts[k])
replace = {}
if len(uniq) > 256:
    kept = uniq[:256]
    kept_arr = np.frombuffer(b"".join(kept), np.uint8).reshape(256, 64)
    for k in uniq[256:]:
        v = np.frombuffer(k, np.uint8)
        replace[k] = kept[int((kept_arr != v).sum(1).argmin())]
    print(f"Reduced unique tiles from {len(uniq)} to 256 by nearest-match merging.")
else:
    print(f"Unique tiles: {len(uniq)} (limit 256).")

final_keys = np.array([replace.get(k, k) for k in keys], dtype=object)
final_tiles = np.stack([np.frombuffer(k, np.uint8) for k in final_keys]).reshape(30, 32, 8, 8)
idx_final = final_tiles.transpose(0, 2, 1, 3).reshape(H, W)

# Final NES-legal image: each pixel = palette[quadrant][index]
out = PAL_RGB[pal_full, idx_final].astype(np.uint8)
Image.fromarray(out, "RGB").save("art.png")

used = np.unique(quad_pal)
print(f"Saved art.png (256x240). Palettes used: {sorted(used.tolist())}")
