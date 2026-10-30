from collections import Counter

import numpy as np
from PIL import Image

W, H = 256, 240
HOR = 150  # horizon line (sky above, lake below)

# Commonly used 2C02 NES master palette (indices 0x00-0x3F)
MASTER = {
    0x00: (124, 124, 124), 0x01: (0, 0, 252), 0x02: (0, 0, 188), 0x03: (68, 40, 188),
    0x04: (148, 0, 132), 0x05: (168, 0, 32), 0x06: (168, 16, 0), 0x07: (136, 20, 0),
    0x08: (80, 48, 0), 0x09: (0, 120, 0), 0x0A: (0, 104, 0), 0x0B: (0, 88, 0),
    0x0C: (0, 64, 88), 0x0D: (0, 0, 0),
    0x10: (188, 188, 188), 0x11: (0, 120, 248), 0x12: (0, 88, 248), 0x13: (104, 68, 252),
    0x14: (216, 0, 204), 0x15: (228, 0, 88), 0x16: (248, 56, 0), 0x17: (228, 92, 16),
    0x18: (172, 124, 0), 0x19: (0, 184, 0), 0x1A: (0, 168, 0), 0x1B: (0, 168, 68),
    0x1C: (0, 136, 136), 0x1D: (0, 0, 0),
    0x20: (248, 248, 248), 0x21: (60, 188, 252), 0x22: (104, 136, 252), 0x23: (152, 120, 248),
    0x24: (248, 120, 248), 0x25: (248, 88, 152), 0x26: (248, 120, 88), 0x27: (252, 160, 68),
    0x28: (248, 184, 0), 0x29: (184, 248, 24), 0x2A: (88, 216, 84), 0x2B: (88, 248, 152),
    0x2C: (0, 232, 216), 0x2D: (120, 120, 120),
    0x30: (252, 252, 252), 0x31: (164, 228, 252), 0x32: (184, 184, 248), 0x33: (216, 184, 248),
    0x34: (248, 184, 248), 0x35: (248, 164, 192), 0x36: (240, 208, 176), 0x37: (252, 224, 168),
    0x38: (248, 216, 120), 0x39: (216, 248, 120), 0x3A: (184, 248, 184), 0x3B: (184, 248, 216),
    0x3C: (0, 252, 252), 0x3D: (248, 216, 248),
}

UNIVERSAL = 0x01  # shared colour 0 of every background palette
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

# ---------------- Tiles (30 x 32 grid of 8x8) ----------------
# Each 8x8 tile lies inside one 16x16 quadrant, so it has one palette.
tile_pal = np.repeat(np.repeat(quad_pal, 2, 0), 2, 1)          # (30,32)
tiles_idx = (idx_full.reshape(30, 8, 32, 8)
             .transpose(0, 2, 1, 3)
             .reshape(30, 32, 64)
             .astype(np.uint8))

# A tile is identified by (palette, pattern): the same pattern in another
# palette renders in different colours, so it is a different tile on screen.
tile_keys = [[(int(tile_pal[r, c]), tiles_idx[r, c].tobytes())
              for c in range(32)] for r in range(30)]
counts = Counter(k for row in tile_keys for k in row)
order = [k for k, _ in counts.most_common()]
print(f"Unique 8x8 tiles (palette+pattern): {len(order)} (limit 256).")

# Keep the most common tiles, guaranteeing at least one tile per palette
# in use so every tile has a same-palette replacement.
kept = []
seen_pal = set()
for k in order:
    if k[0] not in seen_pal:
        seen_pal.add(k[0])
        kept.append(k)
kept_set = set(kept)
for k in order:
    if len(kept) >= 256:
        break
    if k not in kept_set:
        kept.append(k)
        kept_set.add(k)

kept_pal = np.array([p for p, _ in kept], np.int64)
kept_pat = np.stack([np.frombuffer(b, np.uint8) for _, b in kept])        # (K,64)
kept_rgb = PAL_RGB[kept_pal[:, None], kept_pat].astype(np.float32)        # (K,64,3)

# Scene RGB per tile, to judge which replacement looks closest
img_tiles = img.reshape(30, 8, 32, 8, 3).transpose(0, 2, 1, 3, 4).reshape(30, 32, 64, 3)

final_pat = tiles_idx.copy()
merged = 0
for r in range(30):
    for c in range(32):
        k = tile_keys[r][c]
        if k in kept_set:
            continue
        sel = np.where(kept_pal == k[0])[0]          # same palette only
        err = ((kept_rgb[sel] - img_tiles[r, c][None]) ** 2).sum(axis=(1, 2))
        final_pat[r, c] = kept_pat[sel[int(err.argmin())]]
        merged += 1

print(f"Merged {merged} rare tiles into {len(kept)} kept tiles.")

# Back to a full-size index image
idx_final = final_pat.reshape(30, 32, 8, 8).transpose(0, 2, 1, 3).reshape(H, W)

# Final NES-legal image: each pixel = palette[quadrant][index]
out = PAL_RGB[pal_full, idx_final].astype(np.uint8)
Image.fromarray(out, "RGB").save("art.png")

used = np.unique(quad_pal)
print(f"Saved art.png (256x240). Palettes used: {sorted(used.tolist())}")
