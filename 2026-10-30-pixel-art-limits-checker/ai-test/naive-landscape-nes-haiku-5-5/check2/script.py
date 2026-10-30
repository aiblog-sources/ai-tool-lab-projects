import numpy as np
from PIL import Image

W, H = 256, 240
HORIZON = 150                    # lake surface starts here
SUN_X, SUN_Y, SUN_R = 128, 138, 16
OUT = "art.png"
TILE_BUDGET = 256
rng = np.random.default_rng(7)

def rgb(h):
    return tuple(int(h[i:i + 2], 16) for i in (0, 2, 4))

# ---- Drawing palette (full colour, quantized at the end) ----
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

# ---- NES console palette: 1 shared backdrop + 4 palettes x 3 colours ----
BACKDROP = rgb("3E2262")
PALETTES = [
    [rgb("8A3478"), rgb("B94A6A"), rgb("DD6A4E")],   # dusk purple/rose
    [rgb("F29A4A"), rgb("FCC463"), rgb("FFE49A")],   # sun/cloud warm
    [rgb("7A4478"), rgb("9E5E8A"), rgb("3A1E4C")],   # mountains
    [rgb("4A3A7A"), rgb("6E5CA0"), rgb("FFB85A")],   # lake + reflection
]

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


# ---- NES quantization: per 16x16 attribute block, pick one palette ----
# Each block may use the backdrop plus at most 3 colours from ONE palette.
src = img.astype(np.float64)
out = np.zeros((H, W, 3), np.uint8)
block_pal = {}   # (by, bx) -> set of allowed RGB tuples for that block

for by in range(0, H, 16):
    for bx in range(0, W, 16):
        blk = src[by:by + 16, bx:bx + 16]                     # (16,16,3)
        best = None
        for pal in PALETTES:
            cands = np.array([BACKDROP] + pal, np.float64)    # (4,3)
            d = np.sqrt(((blk[:, :, None, :] - cands[None, None, :, :]) ** 2)
                        .sum(-1))                             # (16,16,4)
            err = (np.min(d, axis=-1) ** 2).sum()
            if best is None or err < best[0]:
                best = (err, cands, d)

        _, cands, d = best
        block_pal[(by, bx)] = {tuple(int(v) for v in c) for c in cands}

        order = np.argsort(d, axis=-1)
        i1 = order[..., 0]
        i2 = order[..., 1]
        d1 = np.take_along_axis(d, order[..., :1], -1)[..., 0]
        d2n = np.take_along_axis(d, order[..., 1:2], -1)[..., 0]
        p = d1 / (d1 + d2n + 1e-9)                            # 0 = exactly c1
        Tb = T[by:by + 16, bx:bx + 16]
        pick = np.where(p > Tb, i2, i1)                       # ordered dither
        out[by:by + 16, bx:bx + 16] = cands[pick].astype(np.uint8)


# ---- NES tile budget: at most 256 unique 8x8 tiles ----
def enforce_tile_budget(out, block_pal):
    """Merge rare tiles into existing tiles that fit the block's palette."""
    solid_key = np.tile(np.array(BACKDROP, np.uint8), (8, 8, 1)).tobytes()
    while True:
        groups = {}
        for ty in range(H // 8):
            for tx in range(W // 8):
                k = out[ty * 8:ty * 8 + 8, tx * 8:tx * 8 + 8].tobytes()
                groups.setdefault(k, []).append((ty, tx))
        if len(groups) <= TILE_BUDGET:
            return out

        pool = {}
        for k in groups:
            arr = np.frombuffer(k, np.uint8).reshape(8, 8, 3)
            pool[k] = (arr, set(map(tuple, arr.reshape(-1, 3).tolist())))

        # Solid backdrop tile is always legal, so never pick it as the victim.
        victim = min((k for k in groups if k != solid_key),
                     key=lambda k: len(groups[k]))

        for ty, tx in groups[victim]:
            by, bx = (ty * 8) // 16 * 16, (tx * 8) // 16 * 16
            allowed = block_pal[(by, bx)]
            orig = out[ty * 8:ty * 8 + 8, tx * 8:tx * 8 + 8].astype(np.float64)
            best_k, best_err = solid_key, np.inf
            for k, (arr, cols) in pool.items():
                if k == victim or not cols <= allowed:
                    continue
                err = ((arr.astype(np.float64) - orig) ** 2).sum()
                if err < best_err:
                    best_k, best_err = k, err
            out[ty * 8:ty * 8 + 8, tx * 8:tx * 8 + 8] = \
                np.frombuffer(best_k, np.uint8).reshape(8, 8, 3)


out = enforce_tile_budget(out, block_pal)

Image.fromarray(out, "RGB").save(OUT)
print(f"Saved {OUT} ({W}x{H})")
