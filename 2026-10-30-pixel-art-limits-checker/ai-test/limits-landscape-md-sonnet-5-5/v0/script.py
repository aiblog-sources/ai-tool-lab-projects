import numpy as np
from PIL import Image

W, H = 320, 224
HOR = 144
LV = np.array([0, 36, 72, 109, 145, 182, 218, 255], dtype=np.float64)

def snap(a):
    idx = np.abs(a[..., None] - LV).argmin(-1)
    return LV[idx]

def lerp_stops(t, stops):
    out = np.zeros(t.shape + (3,))
    ts = [s[0] for s in stops]
    for c in range(3):
        out[..., c] = np.interp(t, ts, [s[1][c] for s in stops])
    return out

rng = np.random.RandomState(7)
xs = np.arange(W)
ys = np.arange(HOR)

# sky
t = ys / (HOR - 1)
sky_stops = [(0.0, (30, 20, 70)), (0.35, (110, 40, 110)), (0.65, (225, 90, 70)),
             (0.88, (255, 160, 60)), (1.0, (255, 215, 110))]
col = lerp_stops(t, sky_stops)
scene = np.repeat(col[:, None, :], W, axis=1)

# stars
for _ in range(25):
    sx, sy = rng.randint(0, W), rng.randint(0, 40)
    scene[sy, sx] = (230, 220, 255)

# sun
cx, cy, r = 160, 128, 26
Y, X = np.mgrid[0:HOR, 0:W]
d = np.sqrt((X - cx) ** 2 + (Y - cy) ** 2)
glow = np.clip(1 - (d - r) / 50, 0, 1)[..., None] * 0.35
scene = scene * (1 - glow) + np.array([255, 200, 100]) * glow
sun = d <= r
sc = lerp_stops(np.clip((Y - (cy - r)) / (2 * r), 0, 1), [(0, (255, 250, 200)), (1, (255, 180, 80))])
scene[sun] = sc[sun]

# mountains
far = 100 + 14 * np.sin(xs / 37.0) + 8 * np.sin(xs / 13.0 + 1) + 6 * np.abs(((xs / 23.0) % 2) - 1)
near = 122 + 12 * np.sin(xs / 53.0 + 2) + 6 * np.sin(xs / 17.0) + 5 * np.abs(((xs / 31.0) % 2) - 1)
farm = Y >= far[None, :]
nearm = Y >= near[None, :]
fc = lerp_stops(np.clip((Y - 85) / 60.0, 0, 1), [(0, (95, 45, 105)), (1, (200, 95, 95))])
scene[farm] = fc[farm]
nc = lerp_stops(np.clip((Y - 105) / 40.0, 0, 1), [(0, (30, 18, 55)), (1, (70, 35, 70))])
scene[nearm] = nc[nearm]

# lake (reflection)
lake = np.zeros((H - HOR, W, 3))
for i in range(H - HOR):
    src = HOR - 1 - i
    dx = int(round(1 + (i / 12.0)) * np.sin(i * 0.9 + 1.3)) if i > 0 else 0
    row = np.roll(scene[src], dx, axis=0)
    row = row * 0.72 + np.array([40, 50, 110]) * 0.28
    lake[i] = row
# ripples
for i in range(1, H - HOR, 3):
    xs0 = rng.randint(0, W)
    ln = rng.randint(10, 40)
    for k in range(ln):
        x = (xs0 + k) % W
        lake[i, x] = lake[i, x] * 0.7 + np.array([255, 190, 120]) * 0.3 if abs(x - 160) < 45 else lake[i, x] * 0.8
full = np.vstack([scene, lake])

# dither + snap
bayer = np.array([[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]]) / 16.0 - 0.5
bt = np.tile(bayer, (H // 4, W // 4))[..., None] * 30
full = np.clip(full + bt, 0, 255)
full = snap(full)

# per-band 15-color palettes (bands aligned to 8px tile rows)
bands = [(0, 72), (72, 144), (144, 184), (184, 224)]
out = np.zeros((H, W, 3), dtype=np.uint8)
for y0, y1 in bands:
    band = Image.fromarray(full[y0:y1].astype(np.uint8))
    q = band.quantize(colors=15, method=Image.MEDIANCUT, dither=Image.Dither.NONE)
    pal = np.array(q.getpalette()[:45]).reshape(-1, 3).astype(np.float64)
    pal = np.unique(snap(pal), axis=0)
    px = full[y0:y1].reshape(-1, 3)
    dist = ((px[:, None, :] - pal[None]) ** 2).sum(-1)
    out[y0:y1] = pal[dist.argmin(1)].reshape(y1 - y0, W, 3).astype(np.uint8)

Image.fromarray(out, "RGB").save("art.png")
