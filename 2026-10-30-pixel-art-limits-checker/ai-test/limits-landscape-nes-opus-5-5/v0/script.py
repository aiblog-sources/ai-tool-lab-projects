# NES sunset over mountains and a lake, 256x240.
# Hardware constraints this follows:
#   - Colors come only from the 2C02 NES palette.
#   - There are 4 background palettes of 3 colors each, plus one shared
#     backdrop color (black, $0F).
#   - Each 16x16 attribute area uses a single palette.
#   - The 960 tiles in the nametable use at most 256 unique 8x8 2-bit patterns.
import numpy as np
from PIL import Image

H, W = 240, 256

# NES palette entries used (2C02 RGB approximations)
NES = {
    0x0F: (0, 0, 0),
    0x02: (0, 0, 188),
    0x03: (68, 40, 188),
    0x13: (104, 68, 252),
    0x24: (248, 120, 248),
    0x25: (248, 88, 152),
    0x26: (248, 120, 88),
    0x27: (252, 160, 68),
    0x38: (248, 216, 120),
}
BG = 0x0F
PALETTES = [
    [BG, 0x02, 0x03, 0x13],  # P0 upper sky
    [BG, 0x24, 0x25, 0x26],  # P1 middle sky
    [BG, 0x26, 0x27, 0x38],  # P2 low sky + sun
    [BG, 0x03, 0x27, 0x38],  # P3 far mountains + lake
]

BAYER = (np.array([[0, 8, 2, 10], [12, 4, 14, 6],
                   [3, 11, 1, 9], [15, 7, 13, 5]]) / 16.0) + 1 / 32.0
xs = np.arange(W)


def grad_row(y, stops):
    for (y0, c0), (y1, c1) in zip(stops, stops[1:]):
        if y0 <= y <= y1:
            t = (y - y0) / max(1, (y1 - y0))
            return np.where(t > BAYER[y % 4, xs % 4], c1, c0)
    return np.full(W, stops[-1][1])


img = np.zeros((H, W), dtype=int)

# ---- Sky (only rows 0..159 matter, lake drawn later) ----
for y in range(160):
    if y < 64:
        stops = [(0, 0x02), (32, 0x03), (63, 0x13)]
    elif y < 112:
        stops = [(64, 0x24), (88, 0x25), (111, 0x26)]
    else:
        stops = [(112, 0x26), (134, 0x27), (152, 0x27), (160, 0x38)]
    img[y] = grad_row(y, stops)

# ---- Sun with retro stripes ----
scx, scy, sr = 128, 140, 20
for y in range(scy - sr, 160):
    for x in range(scx - sr, scx + sr + 1):
        if (x - scx) ** 2 + (y - scy) ** 2 <= sr * sr:
            c = 0x38
            if y >= 144:
                k = y - 144
                if (k % 6) < 1 + k // 6:
                    c = 0x27
            img[y, x] = c

# ---- Far mountains ($03) ----
far_pts = [(0, 138), (24, 126), (52, 136), (84, 120), (112, 142), (128, 150),
           (144, 142), (172, 122), (204, 134), (232, 124), (256, 136)]
far = np.interp(xs, [p[0] for p in far_pts], [p[1] for p in far_pts])
far = far.astype(int) + ((xs // 3) % 2)
for x in range(W):
    img[far[x]:160, x] = 0x03

# ---- Near mountains (black silhouettes) ----
lp = [(0, 60), (20, 72), (44, 96), (60, 92), (84, 128), (104, 150), (118, 160)]
rp = [(138, 160), (156, 146), (176, 118), (196, 104), (212, 80), (232, 88), (256, 66)]
near = np.full(W, 999)
lmask, rmask = xs <= 118, xs >= 138
near[lmask] = np.interp(xs[lmask], [p[0] for p in lp], [p[1] for p in lp]).astype(int)
near[rmask] = np.interp(xs[rmask], [p[0] for p in rp], [p[1] for p in rp]).astype(int)
# small pine trees along the shore at the base of the mountains
for tx in list(range(4, 100, 9)) + list(range(160, 252, 9)):
    th = 10 + (tx * 7) % 7
    for dy in range(th):
        hw = dy // 3
        for x in range(tx - hw, tx + hw + 1):
            if 0 <= x < W:
                near[x] = min(near[x], 160 - th + dy) if dy == 0 else near[x]
                img[160 - th + dy, x] = BG
for x in range(W):
    if near[x] < 160:
        img[near[x]:160, x] = BG

# ---- Lake (reflection) ----
above = img[:160].copy()
warm = {0x26: 0x27, 0x27: 0x27, 0x38: 0x38, 0x0F: 0x0F, 0x03: 0x03}
for y in range(160, H):
    d = y - 160
    wob = int(round(1.5 * np.sin(d * 0.7))) if d > 2 else 0
    yp = 159 - d
    src = above[yp, np.clip(xs + wob, 0, W - 1)] if yp >= 0 else np.full(W, 0x03)
    row = np.array([warm.get(c, 0x03) for c in src])
    # ripple lines breaking up the warm reflection
    if d % 4 == 3 and d > 4:
        brk = ((xs + d * 3) // 4) % 3 != 0
        row = np.where(brk & (row == 0x27), 0x03, row)
    # darker water in the foreground
    if d > 44 and d % 4 == 1:
        row = np.where((row == 0x03) & ((xs + d) % 4 < 2), BG, row)
    # sun glitter streak
    if d % 3 != 2:
        w = int(16 - d * 0.12)
        c = scx + wob
        dist = np.abs(xs - c)
        row = np.where(dist < w * 0.5, 0x38, np.where(dist <= w, 0x27, row))
    img[y] = row

# ---- Assign palettes per 16x16 attribute area & map to 2-bit indices ----
def rgb(c):
    return np.array(NES[c], float)

idx = np.zeros((H, W), dtype=np.uint8)
attr = np.zeros((H // 16 + 1, W // 16), dtype=int)
for by in range(0, H, 16):
    for bx in range(0, W, 16):
        blk = img[by:by + 16, bx:bx + 16]
        best, bestp = -1, 0
        for pi, pal in enumerate(PALETTES):
            cov = np.isin(blk, pal[1:]).sum()
            if cov > best:
                best, bestp = cov, pi
        attr[by // 16, bx // 16] = bestp
        pal = PALETTES[bestp]
        prgb = np.array([NES[c] for c in pal], float)
        sub = np.zeros(blk.shape, np.uint8)
        for c in np.unique(blk):
            if c in pal:
                i = pal.index(c)
            else:
                i = int(np.argmin(((prgb - rgb(c)) ** 2).sum(1)))
            sub[blk == c] = i
        idx[by:by + 16, bx:bx + 16] = sub

# ---- Limit to 256 unique tile patterns ----
tiles = idx.reshape(30, 8, 32, 8).transpose(0, 2, 1, 3).reshape(-1, 64)
uniq, inv, cnt = np.unique(tiles, axis=0, return_inverse=True, return_counts=True)
inv = inv.reshape(-1)
n = len(uniq)
remap = np.arange(n)
if n > 256:
    D = (uniq[:, None, :] != uniq[None, :, :]).sum(2).astype(float)
    np.fill_diagonal(D, np.inf)
    cnt = cnt.astype(float)
    alive = n
    while alive > 256:
        mc = np.minimum(cnt[:, None], cnt[None, :])
        Wm = D * (1 + mc)
        i, j = np.unravel_index(np.argmin(Wm), Wm.shape)
        if cnt[i] < cnt[j]:
            i, j = j, i  # merge rarer j into i
        remap[remap == j] = i
        cnt[i] += cnt[j]
        D[j, :] = np.inf
        D[:, j] = np.inf
        alive -= 1
tiles = uniq[remap[inv]]
assert len(np.unique(tiles, axis=0)) <= 256
idx = tiles.reshape(30, 32, 8, 8).transpose(0, 2, 1, 3).reshape(H, W)

# ---- Render final image ----
out = np.zeros((H, W, 3), np.uint8)
for by in range(0, H, 16):
    for bx in range(0, W, 16):
        pal = PALETTES[attr[by // 16, bx // 16]]
        prgb = np.array([NES[c] for c in pal], np.uint8)
        out[by:by + 16, bx:bx + 16] = prgb[idx[by:by + 16, bx:bx + 16]]

Image.fromarray(out, "RGB").save("art.png")
