import numpy as np
from PIL import Image, ImageDraw

W, H = 256, 224
HOR = 120  # horizon line

# ---------- sky ----------
sky_cols = np.array([
    (28, 24, 72), (52, 32, 104), (88, 44, 120), (136, 56, 120),
    (184, 72, 104), (224, 104, 88), (248, 152, 80), (255, 208, 112)
], dtype=float)

bayer = np.array([[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]]) / 16.0

ys, xs = np.mgrid[0:HOR, 0:W]
SUNX, SUNY, SUNR = 128, 104, 15
dist = np.hypot(xs - SUNX, (ys - SUNY) * 1.3)
glow = np.clip(1 - dist / 90, 0, 1)
idx = (ys / (HOR - 1)) * (len(sky_cols) - 1) * 0.92 + glow * 2.2
idx = np.clip(idx, 0, len(sky_cols) - 1)
base = np.floor(idx).astype(int)
frac = idx - base
thr = bayer[ys % 4, xs % 4]
sel = np.where(frac > thr, base + 1, base)
sel = np.clip(sel, 0, len(sky_cols) - 1)
scene = sky_cols[sel].copy()

# stars in the upper sky
rs = np.random.RandomState(7)
for _ in range(45):
    sx, sy = rs.randint(0, W), rs.randint(0, 32)
    scene[sy, sx] = (230, 224, 255) if rs.rand() > 0.3 else (160, 150, 210)

# sun
sun_mask = dist <= SUNR
scene[sun_mask] = (255, 244, 176)
ring = (dist > SUNR) & (dist <= SUNR + 3) & (bayer[ys % 4, xs % 4] > 0.4)
scene[ring] = (255, 224, 140)

# ---------- clouds ----------
def cloud(cx, cy, w, h, seed):
    r = np.random.RandomState(seed)
    wob = r.rand(W) * 2 - 1
    for y in range(max(0, cy - h - 2), min(HOR, cy + h + 3)):
        for x in range(max(0, cx - w - 2), min(W, cx + w + 3)):
            hh = h * (1 + 0.25 * wob[x])
            if ((x - cx) / w) ** 2 + ((y - cy) / hh) ** 2 < 1:
                t = (y - (cy - hh)) / (2 * hh)
                if t < 0.35:
                    c = (255, 176, 136)
                elif t < 0.7:
                    c = (216, 104, 112)
                else:
                    c = (128, 60, 112)
                scene[y, x] = c

cloud(40, 38, 34, 4, 1)
cloud(75, 50, 28, 3, 2)
cloud(205, 34, 36, 4, 3)
cloud(222, 58, 26, 3, 4)
cloud(150, 28, 22, 2, 5)
cloud(20, 66, 20, 2, 6)

# ---------- mountains ----------
def ridge(seed, base_y, amp, sharp=1.0):
    r = np.random.RandomState(seed)
    h = np.zeros(W)
    for spacing, a in ((48, 1.0), (20, 0.5), (8, 0.22), (3, 0.08)):
        n = W // spacing + 3
        pts = r.rand(n)
        px = np.arange(n) * spacing
        v = np.interp(np.arange(W), px, pts)
        if spacing >= 20 and sharp:
            v = 1 - np.abs(2 * v - 1) * 0.6
        h += (v - 0.5) * a
    return (base_y - h * amp).astype(int)

layers = [
    (ridge(11, 84, 38), (136, 70, 124), (200, 100, 110)),
    (ridge(23, 98, 34), (92, 48, 108), (170, 80, 100)),
    (ridge(37, 110, 22), (52, 32, 80), (120, 56, 96)),
]
for top, col, rim in layers:
    for x in range(W):
        t = top[x]
        for y in range(max(t, 0), HOR):
            depth = (y - t) / 40.0
            c = np.array(col, dtype=float)
            c = c * (1 - 0.18 * min(depth, 1))
            if (y - t) > 8 and bayer[y % 4, x % 4] < 0.25 and y > HOR - 10:
                c = c * 0.85 + np.array((200, 90, 100)) * 0.15
            scene[y, x] = c
        if t < HOR:
            scene[t, x] = rim
            if t + 1 < HOR and bayer[(t + 1) % 4, x % 4] > 0.5:
                scene[t + 1, x] = rim
        if x > 0 and top[x] < top[x - 1]:
            for k in range(1, 4):
                if t + k < HOR and bayer[(t + k) % 4, x % 4] > 0.35 * k:
                    scene[t + k, x] = np.array(rim) * 0.75 + np.array(col) * 0.25

scene[HOR - 2:HOR, :] = (36, 24, 64)

# ---------- lake ----------
img = np.zeros((H, W, 3), dtype=float)
img[:HOR] = scene
deep = np.array((18, 30, 88), dtype=float)
rr = np.random.RandomState(99)
for y in range(HOR, H):
    d = y - HOR
    src = int(HOR - 1 - d * 0.85)
    if src < 0:
        src = 0
    amp = 1 + d * 0.025
    for x in range(W):
        off = int(round(np.sin(d * 0.85 + x * 0.03) * amp + np.sin(d * 0.3) * 1.5))
        sx = (x + off) % W
        c = scene[src, sx]
        mix = 0.35 + d / 190.0
        img[y, x] = c * (1 - mix) + deep * mix
    for _ in range(6):
        x0 = rr.randint(0, W)
        ln = rr.randint(4, 14 + d // 6)
        light = rr.rand() > 0.5
        for x in range(x0, min(W, x0 + ln)):
            if light:
                img[y, x] = img[y, x] * 0.7 + np.array((120, 120, 190)) * 0.3
            else:
                img[y, x] = img[y, x] * 0.8
    wd = 4 + d * 0.22
    for x in range(int(SUNX - wd), int(SUNX + wd)):
        if 0 <= x < W and rr.rand() < 0.45 * (1 - abs(x - SUNX) / wd) and (y + x) % 2 == 0 and d < 80:
            img[y, x] = (255, 214, 140) if rr.rand() > 0.3 else (240, 150, 100)

# ---------- foreground ----------
pil = Image.fromarray(np.clip(img, 0, 255).astype(np.uint8), "RGB")
dr = ImageDraw.Draw(pil)
DARK = (14, 16, 40)
MID = (26, 34, 64)
LIGHT = (44, 52, 84)

def shore(side):
    pts = []
    if side == 0:
        for x in range(0, 90):
            yy = 224 - int(34 * max(0, 1 - x / 90) ** 1.4 + 6 * np.sin(x * 0.2))
            pts.append((x, yy))
        pts += [(90, 224), (0, 224)]
    else:
        for x in range(W - 1, 150, -1):
            k = (x - 150) / 106.0
            yy = 224 - int(26 * k ** 1.3 + 4 * np.sin(x * 0.25))
            pts.append((x, yy))
        pts += [(150, 224), (W - 1, 224)]
    dr.polygon(pts, fill=DARK)
    for (x, yy) in pts[:-2]:
        dr.point((x, yy), fill=LIGHT)
        dr.point((x, yy + 1), fill=MID)

shore(0)
shore(1)

def pine(x, base, h):
    w = h // 3 + 2
    dr.rectangle([x - 1, base - 3, x, base + 1], fill=DARK)
    tiers = 4
    for i in range(tiers):
        top = base - h + i * (h // (tiers + 1))
        bot = base - 3 - (tiers - 1 - i) * (h // (tiers + 2))
        half = int(w * (i + 1) / tiers)
        dr.polygon([(x, top), (x - half, bot), (x + half, bot)], fill=DARK)
        dr.line([(x, top), (x - half, bot)], fill=MID)

for (x, b, h) in [(8, 196, 56), (24, 204, 44), (40, 210, 36), (-2, 214, 62), (56, 216, 28)]:
    pine(x, b, h)
for (x, b, h) in [(246, 200, 54), (230, 208, 42), (214, 212, 32), (252, 216, 60), (200, 218, 24)]:
    pine(x, b, h)

for (rx, ry, rw) in [(100, 205, 8), (172, 198, 6), (120, 215, 5)]:
    dr.ellipse([rx - rw, ry - rw // 2, rx + rw, ry + rw // 2], fill=MID)
    dr.arc([rx - rw, ry - rw // 2, rx + rw, ry + rw // 2], 180, 360, fill=LIGHT)

# ---------- SNES constraints ----------
# 15-bit color, 4bpp BG: 8x8 tiles, each tile uses ONE palette of 16 colors,
# 8 palettes total (128 colors), at most 1024 tiles (32x28 = 896 here).
def snap(a):
    a = np.clip(a, 0, 255).astype(np.int32)
    return ((a // 8) * 8 + (a // 8) // 4).astype(np.int32)

arr = snap(np.array(pil))
TW, TH = W // 8, H // 8
tiles = arr.reshape(TH, 8, TW, 8, 3).transpose(0, 2, 1, 3, 4).reshape(TH * TW, 64, 3)

# cluster tiles into 8 palette groups (k-means on mean color + vertical position)
feat = np.concatenate([
    tiles.mean(axis=1),
    tiles.std(axis=1) * 0.5,
    (np.repeat(np.arange(TH), TW)[:, None] * 255.0 / TH) * 0.4,
], axis=1)
rng = np.random.RandomState(3)
NP = 8
cent = [feat[rng.randint(len(feat))]]
for _ in range(NP - 1):
    d2 = np.min([((feat - c) ** 2).sum(1) for c in cent], axis=0)
    cent.append(feat[rng.choice(len(feat), p=d2 / d2.sum())] if d2.sum() > 0 else feat[0])
cent = np.array(cent)
for _ in range(30):
    lab = ((feat[:, None, :] - cent[None]) ** 2).sum(2).argmin(1)
    for k in range(NP):
        m = lab == k
        if m.any():
            cent[k] = feat[m].mean(0)

out_tiles = tiles.copy()
for k in range(NP):
    m = np.where(lab == k)[0]
    if len(m) == 0:
        continue
    px = tiles[m].reshape(-1, 3).astype(np.uint8)
    q = Image.fromarray(px.reshape(1, -1, 3), "RGB").quantize(
        colors=16, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE)
    pal = np.array(q.getpalette()[:48], dtype=np.int32).reshape(-1, 3)
    nused = int(np.array(q).max()) + 1
    pal = snap(pal[:nused])
    for t in m:
        p = tiles[t]
        dd = ((p[:, None, :] - pal[None]) ** 2).sum(2)
        out_tiles[t] = pal[dd.argmin(1)]

res = out_tiles.reshape(TH, TW, 8, 8, 3).transpose(0, 2, 1, 3, 4).reshape(H, W, 3)
Image.fromarray(res.astype(np.uint8), "RGB").save("art.png")
