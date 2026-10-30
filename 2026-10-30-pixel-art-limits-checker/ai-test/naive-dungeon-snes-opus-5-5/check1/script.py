from PIL import Image
import numpy as np
import random

W, H = 256, 224
rng = random.Random(1337)
img = np.zeros((H, W, 3), dtype=np.int32)

def C(r, g, b):
    return np.array([r & 0xF8, g & 0xF8, b & 0xF8], dtype=np.int32)

# ---------------- Palette (SNES-friendly 5-bit steps) ----------------
BLACK      = C(8, 8, 16)
WALL_DK    = C(32, 32, 48)
WALL_MORT  = C(48, 48, 64)
WALL_MID   = C(80, 80, 104)
WALL_MID2  = C(72, 72, 96)
WALL_LT    = C(112, 112, 136)
WALL_HI    = C(152, 152, 176)
FL_GROUT   = C(40, 32, 40)
FL_DK      = C(64, 56, 64)
FL_MID     = C(96, 88, 96)
FL_MID2    = C(88, 80, 88)
FL_LT      = C(128, 120, 120)
WOOD_DK    = C(56, 32, 16)
WOOD_MID   = C(112, 64, 32)
WOOD_LT    = C(160, 104, 48)
IRON_DK    = C(40, 40, 48)
IRON       = C(96, 96, 112)
IRON_LT    = C(152, 152, 168)
FL_RED     = C(200, 48, 16)
FL_ORG     = C(248, 144, 24)
FL_YEL     = C(248, 224, 104)
FL_WHT     = C(248, 248, 216)

def rect(x0, y0, x1, y1, c):
    img[max(0, y0):min(H, y1), max(0, x0):min(W, x1)] = c

def px(x, y, c):
    if 0 <= x < W and 0 <= y < H:
        img[y, x] = c

# Room layout
FX0, FY0, FX1, FY1 = 24, 32, 232, 208   # floor area

# ---------------- Walls (brick pattern everywhere first) ----------------
for by in range(0, H, 8):
    off = 8 if (by // 8) % 2 else 0
    for bx in range(-16, W + 16, 16):
        x0 = bx + off
        base = WALL_MID if rng.random() < 0.6 else WALL_MID2
        rect(x0, by, x0 + 16, by + 8, base)
        rect(x0, by, x0 + 16, by + 1, WALL_LT)
        rect(x0, by + 6, x0 + 16, by + 7, WALL_MORT)
        rect(x0, by + 7, x0 + 16, by + 8, WALL_DK)
        rect(x0, by, x0 + 1, by + 8, WALL_DK)
        for _ in range(rng.randint(0, 2)):
            px(x0 + rng.randint(2, 14), by + rng.randint(2, 5), WALL_MORT)
        if rng.random() < 0.15:
            px(x0 + rng.randint(2, 14), by + rng.randint(1, 4), WALL_HI)

rect(0, 0, W, 3, BLACK)
rect(0, H - 3, W, H, BLACK)
rect(0, 0, 3, H, BLACK)
rect(W - 3, 0, W, H, BLACK)
rect(3, 3, W - 3, 4, WALL_DK)

for y in range(H):
    for x in range(W):
        if (x < FX0 or x >= FX1 or y >= FY1) and y >= FY0:
            if not (FX0 <= x < FX1 and FY0 <= y < FY1):
                img[y, x] = (img[y, x] * 0.82).astype(np.int32)

# ---------------- Floor ----------------
for ty in range(FY0, FY1, 16):
    for tx in range(FX0, FX1, 16):
        base = FL_MID if rng.random() < 0.55 else FL_MID2
        rect(tx, ty, tx + 16, ty + 16, base)
        rect(tx, ty, tx + 15, ty + 1, FL_LT)
        rect(tx, ty, tx + 1, ty + 15, FL_LT)
        rect(tx + 1, ty + 14, tx + 15, ty + 15, FL_DK)
        rect(tx + 14, ty + 1, tx + 15, ty + 15, FL_DK)
        rect(tx, ty + 15, tx + 16, ty + 16, FL_GROUT)
        rect(tx + 15, ty, tx + 16, ty + 16, FL_GROUT)
        for _ in range(rng.randint(2, 5)):
            px(tx + rng.randint(2, 13), ty + rng.randint(2, 13), FL_DK)
        if rng.random() < 0.4:
            px(tx + rng.randint(2, 13), ty + rng.randint(2, 13), FL_LT)
        if rng.random() < 0.12:
            cx, cy = tx + rng.randint(3, 12), ty + rng.randint(3, 12)
            for _ in range(rng.randint(4, 8)):
                px(cx, cy, FL_GROUT)
                cx += rng.choice([-1, 0, 1])
                cy += rng.choice([0, 1])
                cx = min(max(cx, tx + 1), tx + 13)
                cy = min(max(cy, ty + 1), ty + 13)

rect(FX0 - 2, FY0 - 2, FX1 + 2, FY0, WALL_HI)
rect(FX0 - 2, FY0 - 2, FX0, FY1 + 2, WALL_LT)
rect(FX1, FY0 - 2, FX1 + 2, FY1 + 2, WALL_LT)
rect(FX0 - 2, FY1, FX1 + 2, FY1 + 2, WALL_LT)

def darken(x0, y0, x1, y1, f):
    sub = img[y0:y1, x0:x1]
    img[y0:y1, x0:x1] = (sub * f).astype(np.int32)

darken(FX0, FY0, FX1, FY0 + 4, 0.6)
darken(FX0, FY0 + 4, FX1, FY0 + 7, 0.8)
darken(FX0, FY0 + 4, FX0 + 3, FY1, 0.7)
darken(FX1 - 2, FY0 + 4, FX1, FY1, 0.85)

# ---------------- Door (top center) ----------------
DCX, DCY, R = 128, 20, 14
for y in range(0, FY0):
    for x in range(DCX - 24, DCX + 24):
        dx, dy = x - DCX + 0.5, y - DCY + 0.5
        d = (dx * dx + dy * dy) ** 0.5
        in_frame = (y < DCY and R <= d < R + 7) or (y >= DCY and R <= abs(dx) < R + 7)
        if in_frame:
            c = WALL_LT
            if (y < DCY and d > R + 5.5) or (y >= DCY and abs(dx) > R + 5.5):
                c = WALL_DK
            elif (y < DCY and d < R + 1) or (y >= DCY and abs(dx) < R + 1):
                c = WALL_HI
            img[y, x] = c
for ang_x in (-9, 9):
    for t in range(7):
        sx = DCX + int(round(ang_x * (R + t) / R * 0.75))
        sy = DCY - int(round(((R + t) ** 2 - (ang_x * (R + t) / R * 0.75) ** 2) ** 0.5))
        px(sx, sy, WALL_DK)
rect(DCX - 3, DCY - R - 7, DCX + 3, DCY - R, WALL_HI)
rect(DCX - 3, DCY - R - 7, DCX + 3, DCY - R - 6, WALL_LT)
rect(DCX - 4, DCY - R - 7, DCX - 3, DCY - R, WALL_DK)
rect(DCX + 3, DCY - R - 7, DCX + 4, DCY - R, WALL_DK)
for sy in (DCY + 4, DCY + 10):
    rect(DCX - R - 7, sy, DCX - R, sy + 1, WALL_DK)
    rect(DCX + R, sy, DCX + R + 7, sy + 1, WALL_DK)

for y in range(0, FY0):
    for x in range(DCX - R, DCX + R):
        dx, dy = x - DCX + 0.5, y - DCY + 0.5
        inside = (y >= DCY) or (dx * dx + dy * dy < R * R)
        if inside:
            col = (x - (DCX - R)) % 7
            c = WOOD_MID
            if col == 0:
                c = WOOD_DK
            elif col == 1:
                c = WOOD_LT
            img[y, x] = c
for _ in range(30):
    gx = rng.randint(DCX - R + 2, DCX + R - 2)
    gy = rng.randint(DCY - 8, FY0 - 2)
    if (img[gy, gx] == WOOD_MID).all():
        px(gx, gy, WOOD_DK)
        px(gx, gy + 1, WOOD_DK)
for by in (DCY - 6, DCY + 6):
    for x in range(DCX - R, DCX + R):
        dx, dy = x - DCX + 0.5, by - DCY + 0.5
        if by >= DCY or dx * dx + dy * dy < R * R:
            px(x, by, IRON_LT)
            px(x, by + 1, IRON)
            px(x, by + 2, IRON_DK)
    for rx in range(DCX - R + 3, DCX + R - 2, 6):
        dx = rx - DCX + 0.5
        if by >= DCY or dx * dx + (by - DCY) ** 2 < (R - 1) ** 2:
            px(rx, by + 1, IRON_LT)
for (hx, hy) in [(0, -2), (-1, -1), (1, -1), (-2, 0), (2, 0), (-2, 1), (2, 1), (-1, 2), (1, 2), (0, 2)]:
    px(DCX + 7 + hx, DCY + 1 + hy, IRON_DK if hy > 0 else IRON_LT)
px(DCX + 7, DCY - 2, IRON_LT)
rect(DCX - R - 2, FY0 - 1, DCX + R + 2, FY0, WOOD_DK)
rect(DCX - R - 4, FY0, DCX + R + 4, FY0 + 4, WALL_MID)
rect(DCX - R - 4, FY0, DCX + R + 4, FY0 + 1, WALL_HI)
rect(DCX - R - 4, FY0 + 3, DCX + R + 4, FY0 + 4, WALL_DK)
for sx in range(DCX - R - 4, DCX + R + 4, 8):
    rect(sx, FY0, sx + 1, FY0 + 4, WALL_DK)

# ---------------- Torches ----------------
TORCHES = [64, 192]
FLAME_BASE_Y = 12

bayer = np.array([[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]]) / 16.0
warm = np.array([1.35, 1.15, 0.85])
for cx in TORCHES:
    gy = FLAME_BASE_Y - 4
    for y in range(0, 70):
        for x in range(cx - 48, cx + 48):
            d = ((x - cx) ** 2 + ((y - gy) * 1.15) ** 2) ** 0.5
            strength = 1.0 - d / 46.0
            if strength <= 0:
                continue
            if strength > 0.55 or strength > bayer[y % 4, x % 4] * 0.55 + 0.05:
                f = warm if strength > 0.45 else (1 + (warm - 1) * 0.55)
                img[y, x] = np.clip(img[y, x] * f + np.array([10, 4, 0]), 0, 248).astype(np.int32)

FLAME = [
    "...W...",
    "...Y...",
    "..YOY..",
    "..OYO..",
    ".OYWYO.",
    ".OYWYOR",
    "ROYWYOR",
    "ROOYOOR",
    ".RROOR.",
]
FMAP = {"R": FL_RED, "O": FL_ORG, "Y": FL_YEL, "W": FL_WHT}

for cx in TORCHES:
    rect(cx - 3, 20, cx + 3, 29, IRON)
    rect(cx - 3, 20, cx + 3, 21, IRON_LT)
    rect(cx - 3, 20, cx - 2, 29, IRON_LT)
    rect(cx + 2, 21, cx + 3, 29, IRON_DK)
    rect(cx - 3, 28, cx + 3, 29, IRON_DK)
    px(cx - 1, 23, IRON_DK); px(cx, 23, IRON_DK)
    px(cx - 1, 26, IRON_DK); px(cx, 26, IRON_DK)
    rect(cx - 1, 13, cx + 1, 22, WOOD_MID)
    rect(cx - 1, 13, cx, 22, WOOD_LT)
    px(cx, 18, WOOD_DK)
    rect(cx - 3, 11, cx + 3, 14, IRON)
    rect(cx - 3, 11, cx + 3, 12, IRON_LT)
    rect(cx - 2, 14, cx + 2, 15, IRON_DK)
    top = FLAME_BASE_Y - len(FLAME)
    for j, row in enumerate(FLAME):
        for i, ch in enumerate(row):
            if ch in FMAP:
                px(cx - 3 + i, top + j, FMAP[ch])
    px(cx - 3, 29, WALL_DK); px(cx + 2, 29, WALL_DK)

# ---------------- Small floor details ----------------
for _ in range(14):
    x = rng.randint(FX0 + 6, FX1 - 6)
    y = rng.randint(FY0 + 12, FY1 - 6)
    px(x, y, FL_LT)
    px(x + 1, y + 1, FL_GROUT)

for (cx, cy) in [(FX0, FY0), (FX1 - 1, FY0), (FX0, FY1 - 1), (FX1 - 1, FY1 - 1)]:
    for dy in range(-10, 11):
        for dx in range(-10, 11):
            x, y = cx + dx, cy + dy
            if FX0 <= x < FX1 and FY0 <= y < FY1 and dx * dx + dy * dy < 60 and (x + y) % 2 == 0:
                img[y, x] = (img[y, x] * 0.8).astype(np.int32)

# =====================================================================
# SNES constraint pass: 8x8 tiles, 8 palettes x 15 colors + shared backdrop
# =====================================================================
MAX_PALS, PAL_SIZE, T = 8, 15, 8

q = (np.clip(img, 0, 255) >> 3).astype(np.int32)   # 5-bit per channel
keys = (q[..., 0] << 10) | (q[..., 1] << 5) | q[..., 2]

def unpack(k):
    return ((k >> 10) & 31, (k >> 5) & 31, k & 31)

def d2(a, b):
    ra, ga, ba = unpack(a)
    rb, gb, bb = unpack(b)
    return 2 * (ra - rb) ** 2 + 4 * (ga - gb) ** 2 + 3 * (ba - bb) ** 2

vals, cnts = np.unique(keys, return_counts=True)
BACKDROP = int(vals[np.argmax(cnts)])

def reduce_set(cl, k):
    """Merge closest colors (weighted by pixel count) until len <= k."""
    cl = dict(cl)
    cost = 0
    while len(cl) > k:
        items = list(cl.items())
        best = None
        for i in range(len(items)):
            ci, wi = items[i]
            for j in range(i + 1, len(items)):
                cj, wj = items[j]
                c = min(wi, wj) * d2(ci, cj)
                if best is None or c < best[0]:
                    best = (c, ci, wi, cj, wj)
        c, ci, wi, cj, wj = best
        keep, drop = (ci, cj) if wi >= wj else (cj, ci)
        cl[keep] += cl.pop(drop)
        cost += c
    return cl, cost

# Per-tile color sets (excluding backdrop), each reduced to <= 15
TH, TW = H // T, W // T
clusters = {}
for ty in range(TH):
    for tx in range(TW):
        blk = keys[ty * T:(ty + 1) * T, tx * T:(tx + 1) * T].ravel()
        u, c = np.unique(blk, return_counts=True)
        d = {int(a): int(b) for a, b in zip(u, c) if int(a) != BACKDROP}
        if not d:
            continue
        if len(d) > PAL_SIZE:
            d, _ = reduce_set(d, PAL_SIZE)
        fk = frozenset(d)
        if fk in clusters:
            for a, b in d.items():
                clusters[fk][a] += b
        else:
            clusters[fk] = dict(d)

# Absorb subsets into supersets
sets = sorted(clusters.items(), key=lambda kv: -len(kv[0]))
kept = []
for fk, d in sets:
    host = None
    for kfk, kd in kept:
        if fk <= kfk:
            host = kd
            break
    if host is not None:
        for a, b in d.items():
            host[a] += b
    else:
        kept.append((fk, d))
pals = [d for _, d in kept]

def merge_dicts(a, b):
    m = dict(a)
    for k, v in b.items():
        m[k] = m.get(k, 0) + v
    return m

# Lossless merging first
while len(pals) > MAX_PALS:
    best = None
    for i in range(len(pals)):
        si = set(pals[i])
        for j in range(i + 1, len(pals)):
            u = si | set(pals[j])
            if len(u) <= PAL_SIZE:
                score = (len(u) - max(len(si), len(pals[j])), -len(si & set(pals[j])))
                if best is None or score < best[0]:
                    best = (score, i, j)
    if best is None:
        break
    _, i, j = best
    m = merge_dicts(pals[i], pals[j])
    pals = [p for k, p in enumerate(pals) if k not in (i, j)] + [m]

# Lossy merging if still too many
while len(pals) > MAX_PALS:
    best = None
    for i in range(len(pals)):
        for j in range(i + 1, len(pals)):
            m = merge_dicts(pals[i], pals[j])
            r, cost = reduce_set(m, PAL_SIZE)
            if best is None or cost < best[0]:
                best = (cost, i, j, r)
    _, i, j, r = best
    pals = [p for k, p in enumerate(pals) if k not in (i, j)] + [r]

# Palette arrays (with backdrop) in 5-bit RGB
pal_arrays = []
for p in pals:
    ks = [BACKDROP] + list(p.keys())
    pal_arrays.append(np.array([unpack(k) for k in ks], dtype=np.int32))

wts = np.array([2, 4, 3], dtype=np.int32)
out5 = q.copy()
for ty in range(TH):
    for tx in range(TW):
        blk = q[ty * T:(ty + 1) * T, tx * T:(tx + 1) * T].reshape(-1, 3)
        best = None
        for P in pal_arrays:
            dist = (((blk[:, None, :] - P[None, :, :]) ** 2) * wts).sum(axis=2)
            idx = dist.argmin(axis=1)
            err = dist[np.arange(len(blk)), idx].sum()
            if best is None or err < best[0]:
                best = (err, P[idx])
            if err == 0:
                break
        out5[ty * T:(ty + 1) * T, tx * T:(tx + 1) * T] = best[1].reshape(T, T, 3)

# Expand 5-bit to 8-bit exactly as the console does
out = ((out5 << 3) | (out5 >> 2)).astype(np.uint8)
Image.fromarray(out, "RGB").save("art.png")
