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

# ---------------- Walls ----------------
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
# SNES BG conversion: 8x8 tiles, 8 palettes x (shared color 0 + 15 colors)
# =====================================================================
base = (np.clip(img, 0, 255).astype(np.int32) & 0xF8)
TW, TH = W // 8, H // 8
NT = TW * TH
tiles = base.reshape(TH, 8, TW, 8, 3).transpose(0, 2, 1, 3, 4).reshape(NT, 64, 3).astype(np.float64)

SHARED = BLACK.astype(np.float64)   # palette index 0, common to all palettes
NPAL, NCOL = 8, 15
WGT = np.array([1.0, 1.3, 0.8])     # mild perceptual weighting

def snap(a):
    return np.clip(np.round(np.asarray(a) / 8.0) * 8, 0, 248)

def kmeans_colors(pix, k):
    cols, counts = np.unique(pix.reshape(-1, 3), axis=0, return_counts=True)
    keep = ~np.all(cols == SHARED, axis=1)
    cols, counts = cols[keep].astype(np.float64), counts[keep].astype(np.float64)
    if len(cols) == 0:
        return np.zeros((0, 3))
    if len(cols) <= k:
        return cols
    # deterministic weighted farthest-point init
    cent = [cols[np.argmax(counts)]]
    for _ in range(k - 1):
        d = np.min([(((cols - c) * WGT) ** 2).sum(1) for c in cent], axis=0)
        cent.append(cols[np.argmax(d * counts)])
    cent = np.array(cent)
    for _ in range(12):
        d = (((cols[:, None, :] - cent[None]) * WGT) ** 2).sum(-1)
        lab = d.argmin(1)
        for j in range(k):
            m = lab == j
            if m.any():
                cent[j] = (cols[m] * counts[m, None]).sum(0) / counts[m].sum()
    cent = np.unique(snap(cent), axis=0)
    return cent

def full_pal(cols):
    p = np.vstack([SHARED[None], cols]) if len(cols) else SHARED[None]
    if len(p) < 16:
        p = np.vstack([p, np.repeat(SHARED[None], 16 - len(p), 0)])
    return p[:16]

def tile_err(pal):
    d = (((tiles[:, :, None, :] - pal[None, None]) * WGT) ** 2).sum(-1)
    return d.min(-1).sum(-1), d.argmin(-1)

# initial grouping: k-means on tile mean color (warm glow / wood / stone separate)
means = tiles.mean(1)
init = [means[0]]
for _ in range(NPAL - 1):
    d = np.min([((means - c) ** 2).sum(1) for c in init], axis=0)
    init.append(means[np.argmax(d)])
init = np.array(init)
for _ in range(10):
    lab = ((means[:, None] - init[None]) ** 2).sum(-1).argmin(1)
    for j in range(NPAL):
        if (lab == j).any():
            init[j] = means[lab == j].mean(0)
assign = lab

pals = [full_pal(np.zeros((0, 3))) for _ in range(NPAL)]
for it in range(8):
    for g in range(NPAL):
        members = np.where(assign == g)[0]
        if len(members):
            pals[g] = full_pal(kmeans_colors(tiles[members], NCOL))
    errs = np.stack([tile_err(p)[0] for p in pals], 1)
    new_assign = errs.argmin(1)
    # reseed empty palettes from the worst-fit tile
    for g in range(NPAL):
        if not (new_assign == g).any():
            worst = errs[np.arange(NT), new_assign].argmax()
            new_assign[worst] = g
    if np.array_equal(new_assign, assign) and it > 0:
        break
    assign = new_assign

for g in range(NPAL):
    members = np.where(assign == g)[0]
    if len(members):
        pals[g] = full_pal(kmeans_colors(tiles[members], NCOL))
errs_idx = [tile_err(p) for p in pals]
errs = np.stack([e[0] for e in errs_idx], 1)
assign = errs.argmin(1)

out_tiles = np.zeros_like(tiles)
for t in range(NT):
    g = assign[t]
    out_tiles[t] = pals[g][errs_idx[g][1][t]]

out = out_tiles.reshape(TH, TW, 8, 8, 3).transpose(0, 2, 1, 3, 4).reshape(H, W, 3)
out = (snap(out).astype(np.uint8)) & 0xF8

# sanity checks
ot = out.reshape(TH, 8, TW, 8, 3).transpose(0, 2, 1, 3, 4).reshape(NT, 64, 3)
for t in range(NT):
    assert len(np.unique(ot[t], axis=0)) <= 16
assert len(np.unique(out.reshape(-1, 3), axis=0)) <= NPAL * NCOL + 1

Image.fromarray(out, "RGB").save("art.png")
