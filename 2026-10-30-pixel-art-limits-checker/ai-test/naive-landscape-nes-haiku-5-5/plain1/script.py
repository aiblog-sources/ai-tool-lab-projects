import numpy as np
from PIL import Image

W, H = 256, 240
HORIZON = 150                    # lake surface starts here
SUN_X, SUN_Y, SUN_R = 128, 138, 16
OUT = "art.png"
rng = np.random.default_rng(7)

def rgb(h):
    return tuple(int(h[i:i + 2], 16) for i in (0, 2, 4))

# ---- NES background colour limits ----
# 1 shared backdrop colour + four 3-colour sub-palettes = 13 colours on screen.
# Each 16x16 attribute block uses one sub-palette, so each 8x8 tile shows
# at most 4 colours (backdrop + 3).
BACKDROP = rgb("5E2A74")
SUB_PALETTES = np.array([
    [rgb("2B1A4A"), rgb("3E2262"), rgb("8A3478")],   # deep sky
    [rgb("B94A6A"), rgb("DD6A4E"), rgb("F29A4A")],   # warm sky / clouds
    [rgb("FCC463"), rgb("FFE49A"), rgb("FFF6C8")],   # sun / glow
    [rgb("7A4478"), rgb("3A1E4C"), rgb("4A3A7A")],   # mountains / lake
], np.float64)

# Full-colour drawing palette (quantized to the NES limits at the end)
SKY = np.array([rgb(c) for c in ["2B1A4A", "3E2262", "5E2A74", "8A3478",
                                 "B94A6A", "DD6A4E", "F29A4A", "FCC463",
                                 "FFE49A"]], np.uint8)
LAKE = np.array([rgb(c) for c in ["5A4A8A", "4A3A7A", "3E2E6A", "33245C"]], np.uint8)
SUN, SUN_RIM = rgb("FFF6C8"), rgb("FFD070")
CLOUD, CLOUD_HI = rgb("F5A08A"), rgb("FFD0A0")
FAR, FAR_HI = rgb("7A4478"), rgb("9E5E8A")
NEAR, NEAR_HI = rgb("3A1E4C"), rgb("5E3670")
REFL = rgb("FFB85A")
SHIMMER = rgb("6E5CA0")

# 4x4 Bayer threshold map for ordered dithering
B4 = np.array([[0, 8, 2, 10],
               [12, 4, 14, 6],
               [3, 11, 1, 9],
               [15, 7, 13, 5]]) / 16.0
T = np.tile(B4, (H // 4 + 1, W // 4 + 1))[:H, :W]

ys = np.arange(H)[:, None]   # (H,1)
xs = np.arange(W)[None, :]   # (1,W)


def dithered_index(t, n):
    """Map float palette positions to indices, dithering between neighbours."""
    base = np.floor(t).astype(int)
    frac = t - base
    return np.clip(base + (frac > T), 0, n - 1)


def ridge_top(seed, base, amp):
    """Per-column mountain top height built from sharp-peaked waves."""
    r = np.random.default_rng(seed)
    x = np.arange(W)
    freqs = r.uniform(0.025, 0.07, 4)
    phases = r.uniform(0, 2 * np.pi, 4)
    weights = [0.4, 0.3, 0.2, 0.1]
    s = np.zeros(W)
    for w, f, p in zip(weights, freqs, phases):
        s += w * (1 - np.abs(np.sin(x * f + p)))   # sharp peaks, rounded valleys
    return base - amp * s                            # shape (W,)


# ---- Sky ----
sky_t = np.clip(ys / HORIZON, 0, 0.999) * (len(SKY) - 1)
img = SKY[dithered_index(sky_t, len(SKY))]          # (H,W,3)

# ---- Clouds ----
def cloud_mask(cx, cy, w):
    m = np.zeros((H, W), bool)
    for ox, rw in [(0, w / 2), (w * 0.25, w / 3)]:
        m |= ((xs - (cx + ox)) / rw) ** 2 + ((ys - cy) / 2.5) ** 2 <= 1
    return m

for cx, cy, w in [(50, 52, 44), (190, 70, 56), (120, 30, 36), (30, 100, 40)]:
    m = cloud_mask(cx, cy, w)
    img[m] = CLOUD
    img[m & (ys < cy)] = CLOUD_HI

# ---- Sun with halftone glow ----
d2 = (xs - SUN_X) ** 2 + (ys - SUN_Y) ** 2
halo = (d2 <= (SUN_R + 10) ** 2) & (d2 > SUN_R ** 2) & (T < 0.5) & (ys < HORIZON)
img[halo] = SKY[-1]
img[d2 <= SUN_R ** 2] = SUN
img[(d2 <= SUN_R ** 2) & (d2 > (SUN_R - 2) ** 2)] = SUN_RIM

# ---- Mountains ----
far_top = ridge_top(11, 128, 45)
near_top = ridge_top(21, 142, 30)

far = ys >= far_top[None, :]
img[far] = FAR
img[far & (ys < far_top[None, :] + 2)] = FAR_HI

near = ys >= near_top[None, :]
img[near] = NEAR
img[near & (ys < near_top[None, :] + 2)] = NEAR_HI

# ---- Lake ----
LM = np.broadcast_to(ys >= HORIZON, (H, W))
lake_t = np.clip((ys - HORIZON) / (H - HORIZON), 0, 0.999) * (len(LAKE) - 1)
lake_img = LAKE[dithered_index(lake_t, len(LAKE))]
img[LM] = lake_img[LM]

# Shimmer lines on the water
dy = ys - HORIZON                                     # (H,1)
shim = LM & ((dy % 7) == 3) & ((xs % 9) < 5)
img[shim] = SHIMMER

# Mirrored mountain reflections (fade out with depth)
src_row = HORIZON - 1 - dy                            # (H,1)
near_refl = LM & (dy >= 0) & (dy < 14) & (src_row >= near_top[None, :])
far_refl = LM & (dy >= 0) & (dy < 10) & (src_row >= far_top[None, :]) & ~near_refl
img[near_refl & (T < (1 - dy / 14.0))] = NEAR
img[far_refl & (T < (1 - dy / 10.0))] = FAR

# Sun reflection: rippling column that narrows with distance
half = np.maximum(0, 15 - dy * 0.22)                  # (H,1)
ripple = np.sin(dy * 0.9) * 2                         # (H,1)
band = (np.abs(xs - SUN_X - ripple) < half)
stripes = LM & (dy >= 1) & (dy < 60) & band & ((dy % 3) != 2)
img[stripes] = REFL

sparkle = (LM & (dy >= 1) & (dy < 60) & (np.abs(xs - SUN_X) < half * 1.6)
           & ((dy % 3) == 2) & (rng.random((H, W)) < 0.35))
img[sparkle] = REFL


# ---- NES constraint pass ----
def quantize_nes(src):
    """Quantize each 16x16 attribute block to backdrop + one sub-palette."""
    src = src.astype(np.float64)
    out = np.zeros((H, W, 3), np.float64)
    backdrop = np.array(BACKDROP, np.float64)
    for by in range(H // 16):
        for bx in range(W // 16):
            rs = slice(by * 16, by * 16 + 16)
            cs = slice(bx * 16, bx * 16 + 16)
            blk = src[rs, cs].reshape(-1, 3)
            best_err, best_pix = None, None
            for sp in SUB_PALETTES:
                pal = np.vstack([backdrop, sp])                     # (4,3)
                d = ((blk[:, None, :] - pal[None, :, :]) ** 2).sum(-1)  # (256,4)
                idx = d.argmin(1)
                err = d[np.arange(len(blk)), idx].sum()
                if best_err is None or err < best_err:
                    best_err, best_pix = err, pal[idx]
            out[rs, cs] = best_pix.reshape(16, 16, 3)
    return out.astype(np.uint8)


img = quantize_nes(img)

# ---- Verify limits ----
all_colors = np.unique(img.reshape(-1, 3), axis=0)
max_tile = 0
for ty in range(0, H, 8):
    for tx in range(0, W, 8):
        t = img[ty:ty + 8, tx:tx + 8].reshape(-1, 3)
        max_tile = max(max_tile, len(np.unique(t, axis=0)))
assert len(all_colors) <= 25 and max_tile <= 4, "NES limits violated"

Image.fromarray(img, "RGB").save(OUT)
print(f"Saved {OUT} ({W}x{H}), {len(all_colors)} colours, "
      f"max {max_tile} colours per 8x8 tile")
