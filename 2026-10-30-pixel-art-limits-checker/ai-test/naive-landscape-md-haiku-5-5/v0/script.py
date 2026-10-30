import random
import numpy as np
from PIL import Image

W, H = 320, 224          # Mega Drive screen
HORIZON = 136            # water line / mountain base
OUT = "art.png"

# Ordered (Bayer 4x4) dither matrix used when snapping gradients to Genesis levels
BAYER = np.array([[0, 8, 2, 10],
                  [12, 4, 14, 6],
                  [3, 11, 1, 9],
                  [15, 7, 13, 5]]) / 16.0


def c(r, g, b):
    """Color from Genesis-legal channel values (multiples of 36)."""
    return np.array([r, g, b], dtype=float)


def grad(stops_t, stops_c, t):
    """Piecewise-linear gradient per channel."""
    return np.stack(
        [np.interp(t, stops_t, [s[k] for s in stops_c]) for k in range(3)],
        axis=1,
    )


def dither(img):
    """Snap float RGB to 3-bit-per-channel Genesis levels (0..7 * 36) with dithering."""
    ys, xs = np.mgrid[0:H, 0:W]
    thr = BAYER[ys % 4, xs % 4][..., None]
    lv = img / 36.0
    lo = np.floor(lv)
    frac = lv - lo
    lv = lo + (frac > thr)
    return (np.clip(lv, 0, 7) * 36).astype(np.uint8)


def ridge_profile(rng, rough=0.55, n_pow=6):
    """Midpoint-displacement mountain profile, normalized to 0..1 across the screen."""
    n = 2 ** n_pow + 1
    h = np.zeros(n)
    h[0], h[-1] = rng.uniform(-1, 1), rng.uniform(-1, 1)
    step, scale = n - 1, 1.0
    while step > 1:
        half = step // 2
        for i in range(half, n - 1, step):
            h[i] = (h[i - half] + h[i + half]) / 2 + rng.uniform(-scale, scale)
        scale *= rough
        step = half
    h = (h - h.min()) / (h.max() - h.min())
    xs = np.linspace(0, n - 1, W)
    return np.interp(xs, np.arange(n), h)


def draw_range(img, rng, base, amp, color, rim=None):
    prof = ridge_profile(rng)
    tops = (base - amp * prof).astype(int)
    for x in range(W):
        y0 = tops[x]
        img[y0:HORIZON, x] = color
        if rim is not None:
            img[y0, x] = rim


def main():
    rng = random.Random(7)
    img = np.zeros((H, W, 3), dtype=float)
    yy, xx = np.mgrid[0:H, 0:W]

    # --- Sky: deep indigo at top -> magenta -> orange -> yellow at horizon ---
    sky_t = [0.0, 0.45, 0.75, 1.0]
    sky_c = [c(36, 36, 108), c(144, 72, 144), c(252, 144, 72), c(252, 216, 108)]
    t = np.arange(HORIZON) / (HORIZON - 1)
    sky = grad(sky_t, sky_c, t)
    img[:HORIZON] = sky[:, None, :]

    # --- Sun: half-hidden disk with scanline gaps near the horizon ---
    sun_cy = HORIZON - 8
    disk = (xx - 160) ** 2 + (yy - sun_cy) ** 2 <= 26 ** 2
    stripes = (yy > HORIZON - 30) & ((yy // 3) % 2 == 1)
    img[disk & ~stripes] = c(252, 216, 108)

    # --- Mountain ranges (far -> mid -> near) ---
    draw_range(img, rng, base=100, amp=60, color=c(144, 72, 144),
               rim=c(216, 144, 180))
    draw_range(img, rng, base=118, amp=40, color=c(72, 36, 108))
    draw_range(img, rng, base=128, amp=22, color=c(36, 36, 72))

    # --- Lake: sky-colored gradient below the horizon ---
    lake_h = H - HORIZON
    lake_t = [0.0, 0.35, 0.7, 1.0]
    lake_c = [c(252, 180, 108), c(180, 108, 144), c(72, 72, 144), c(36, 36, 108)]
    lt = np.arange(lake_h) / (lake_h - 1)
    lake = grad(lake_t, lake_c, lt)
    img[HORIZON:] = lake[:, None, :]

    # Horizontal ripple lines, darker in the lower lake
    for y in range(HORIZON + 20, H):
        if (y - HORIZON) % 6 == 5:
            img[y] *= 0.85

    # Sun reflection: shimmering column that narrows with distance
    w_row = np.maximum(0, 30 - (np.arange(H) - HORIZON) * 0.35)
    refl = (np.abs(xx - 160) < w_row[:, None]) & \
           (((yy - HORIZON) % 4) < 2) & (yy > HORIZON)
    img[refl] = c(252, 216, 108)

    # --- Dark foreground shore in the bottom-left corner ---
    for x in range(0, 110):
        top = int(H - 60 + (x / 110.0) ** 2 * 60)
        img[top:, x] = c(36, 72, 72)
        img[top, x] = c(72, 108, 108)

    out = dither(img)
    Image.fromarray(out, "RGB").save(OUT)


if __name__ == "__main__":
    main()
