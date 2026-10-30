import math, random
from collections import Counter
import numpy as np
from PIL import Image

W, H = 256, 224
HORIZON = 128

# ---------------- SNES color helpers (BGR555) ----------------
def s5(v):
    return max(0, min(31, int(round(v / 255 * 31))))

def expand(s):
    return (s << 3) | (s >> 2)

def snap(rgb):
    return tuple(expand(s5(v)) for v in rgb)

GP = []  # global list of (snapped) colors
def add(rgb):
    c = snap(rgb)
    if c not in GP:
        GP.append(c)
    return GP.index(c)

SHARED = add((16, 8, 32))  # color 0 of every palette (backdrop / tree line)

BAYER = [[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]]
def dith(frac, x, y):
    return frac > (BAYER[y & 3][x & 3] + 0.5) / 16.0

random.seed(7)
img = np.zeros((H, W), dtype=int)

# ---------------- Sky ----------------
stops = [(0, (20, 12, 56)), (28, (56, 24, 96)), (56, (124, 40, 112)),
         (80, (200, 72, 96)), (100, (240, 128, 64)), (127, (252, 200, 112))]
def stopcol(y):
    for (y0, c0), (y1, c1) in zip(stops, stops[1:]):
        if y0 <= y <= y1:
            t = (y - y0) / (y1 - y0)
            return tuple(a + (b - a) * t for a, b in zip(c0, c1))
    return stops[-1][1]

L = 14
skyc = [add(stopcol(i * 127 / (L - 1))) for i in range(L)]
SUN_X, SUN_Y, SUN_R = 128, 104, 19

for y in range(HORIZON):
    for x in range(W):
        f = y / 127 * (L - 1)
        d = math.hypot(x - SUN_X, (y - SUN_Y) * 1.3)
        f += 2.5 * max(0.0, 1 - d / 85) ** 2
        f = min(f, L - 1)
        lo = int(f)
        hi = min(lo + 1, L - 1)
        img[y, x] = skyc[hi] if dith(f - lo, x, y) else skyc[lo]

# stars
star = add((200, 184, 224))
for _ in range(28):
    sx, sy = random.randrange(W), random.randrange(0, 34)
    img[sy, sx] = star

# ---------------- Sun ----------------
sun_core = add((252, 252, 216))
sun_mid = add((252, 232, 150))
sun_org = add((248, 200, 96))
gaps = {1, 4, 5, 8, 9, 10, 13, 14, 15}
for y in range(SUN_Y - SUN_R, SUN_Y + SUN_R + 1):
    k = y - (SUN_Y + 3)
    if k in gaps:
        continue
    for x in range(SUN_X - SUN_R, SUN_X + SUN_R + 1):
        d = math.hypot(x - SUN_X, y - SUN_Y)
        if d <= SUN_R:
            img[y, x] = sun_mid if d > SUN_R - 3.5 else sun_core

# ---------------- Clouds ----------------
hi_body, hi_lit = add((110, 50, 120)), add((200, 90, 140))
lo_body, lo_lit = add((190, 80, 100)), add((248, 160, 110))
clouds = [(40, 60, 40, 3), (33, 190, 50, 2.5), (58, 222, 36, 3),
          (66, 36, 30, 2.5), (80, 160, 46, 2.2), (50, 118, 24, 2)]
for cy, cx, hl, th in clouds:
    body, lit = (hi_body, hi_lit) if cy < 55 else (lo_body, lo_lit)
    for x in range(max(0, int(cx - hl)), min(W, int(cx + hl) + 1)):
        u = (x - cx) / hl
        if abs(u) >= 1:
            continue
        t = th * (1 - u * u)
        c = cy + 1.5 * math.sin(x * 0.15 + cy)
        top, bot = c - t, c + t * 0.5
        for y in range(int(math.ceil(top)), int(bot) + 1):
            if 0 <= y < HORIZON:
                img[y, x] = lit if y >= bot - 1 else body

# ---------------- Mountains ----------------
def draw_range(hfun, base, lit, rim, shade, rim_depth, lit_depth):
    for x in range(W):
        h = hfun(x)
        litside = (hfun(x + 1) > hfun(x - 1)) if x < SUN_X else (hfun(x + 1) < hfun(x - 1))
        for y in range(max(0, int(math.ceil(h))), HORIZON):
            dpt = y - h
            if litside:
                if dpt < rim_depth:
                    c = rim
                elif dpt < lit_depth:
                    c = lit
                elif dpt < lit_depth + 5:
                    c = base if dith((dpt - lit_depth) / 5, x, y) else lit
                else:
                    c = base
            else:
                c = shade if dpt > 1 else base
            img[y, x] = c

def hf(x):
    a = 0.35 + 0.65 * abs(x - 128) / 128
    return HORIZON - a * (40 + 12 * math.sin(x * 0.029 + 1.3)
                          + 7 * math.sin(x * 0.071 + 0.4) + 3 * math.sin(x * 0.17 + 2))

def hn(x):
    return HORIZON - (6 + 78 * (abs(x - 128) / 128) ** 1.6
                      + 5 * math.sin(x * 0.13 + 0.7) + 3 * math.sin(x * 0.37))

draw_range(hf, add((100, 60, 124)), add((164, 84, 124)), add((232, 120, 96)),
           add((76, 44, 104)), 1.5, 10)
draw_range(hn, add((44, 28, 72)), add((72, 40, 88)), add((208, 100, 88)),
           add((36, 22, 60)), 1.2, 6)

# ---------------- Tree line ----------------
trees = [(cx + random.randint(-1, 1), random.randint(3, 10)) for cx in range(-4, W + 4, 4)]
for x in range(W):
    m = max(max(0, h - 2 * abs(x - cx)) for cx, h in trees)
    top = 125 - m
    for y in range(top, HORIZON):
        img[y, x] = SHARED

# ---------------- Lake (reflection) ----------------
water_map = {}
def wmap(i):
    if i == SHARED:
        return SHARED
    if i not in water_map:
        r, g, b = GP[i]
        water_map[i] = add((r * 0.55, g * 0.58, b * 0.72 + 12))
    return water_map[i]

land = img[:HORIZON].copy()
for y in range(HORIZON, H):
    dy = y - HORIZON
    src = max(0, 127 - int(dy * 1.15))
    ox = int(round(1.5 * math.sin(dy * 0.8) * (0.5 + dy / 60)))
    for x in range(W):
        sx = min(W - 1, max(0, x + ox))
        o = land[src, sx]
        if dy % 3 == 0 and ((x + dy * 3) // 6 + dy) % 11 == 0:
            img[y, x] = o
        else:
            img[y, x] = wmap(o)

# sun glitter
rng = random.Random(3)
for y in range(HORIZON + 1, H, 2):
    dy = y - HORIZON
    w = 6 + dy * 0.35
    for _ in range(3):
        ln = rng.randint(2, 6)
        x0 = int(SUN_X + rng.uniform(-w, w) - ln / 2)
        for x in range(x0, x0 + ln):
            if 0 <= x < W:
                dd = abs(x - SUN_X) / w
                img[y, x] = sun_core if dd < 0.3 else (sun_mid if dd < 0.65 else sun_org)

# ---------------- Palette packing (8 palettes x 15 colors + shared color 0) ----------------
NPAL, PCOL = 8, 15
GPA = np.array(GP, dtype=float)
def cdist(a, b):
    d = GPA[a] - GPA[b]
    return 0.3 * d[0] ** 2 + 0.59 * d[1] ** 2 + 0.11 * d[2] ** 2

TX, TY = W // 8, H // 8
tiles, counts = [], []
for ty in range(TY):
    for tx in range(TX):
        blk = img[ty * 8:ty * 8 + 8, tx * 8:tx * 8 + 8]
        tiles.append((tx, ty))
        counts.append(Counter(int(v) for v in blk.flat if v != SHARED))

# Pre-reduce tiles with >15 colors (merge rarest into nearest kept color)
for i, cnt in enumerate(counts):
    while len(cnt) > PCOL:
        rare = min(cnt, key=lambda c: cnt[c])
        n = cnt.pop(rare)
        tgt = min(cnt, key=lambda c: cdist(rare, c))
        cnt[tgt] += n

def tile_err(cnt, pal):
    opts = list(pal) + [SHARED]
    return sum(n * min(cdist(c, o) for o in opts) for c, n in cnt.items())

# Greedy initial packing
pals, assign = [], [0] * len(tiles)
order = sorted(range(len(tiles)), key=lambda i: -len(counts[i]))
for i in order:
    cnt = counts[i]
    cs = set(cnt)
    best = None
    for p, pal in enumerate(pals):
        need = len(cs - set(pal))
        if len(pal) + need <= PCOL and (best is None or need < best[0]):
            best = (need, p)
    if best is not None:
        p = best[1]
        pals[p] = pals[p] + sorted(cs - set(pals[p]))
    elif len(pals) < NPAL:
        pals.append(sorted(cs))
        p = len(pals) - 1
    else:
        bestp = None
        for p, pal in enumerate(pals):
            room = PCOL - len(pal)
            missing = [c for c, _ in cnt.most_common() if c not in pal]
            cand = pal + missing[:room]
            err = tile_err(cnt, cand)
            if bestp is None or err < bestp[0]:
                bestp = (err, p, cand)
        _, p, cand = bestp
        pals[p] = cand
    assign[i] = p

# Refinement: rebuild each palette from its tiles' usage, then reassign tiles
for _ in range(4):
    newpals = []
    for p in range(len(pals)):
        usage = Counter()
        for i, a in enumerate(assign):
            if a == p:
                usage.update(counts[i])
        newpals.append([c for c, _ in usage.most_common(PCOL)] if usage else pals[p])
    pals = newpals
    assign = [min(range(len(pals)), key=lambda p: tile_err(counts[i], pals[p]))
              for i in range(len(tiles))]

while len(pals) < NPAL:
    pals.append([])
assert len(pals) <= NPAL and all(len(p) <= PCOL for p in pals)

# ---------------- Build indexed output ----------------
out = np.zeros((H, W), dtype=np.uint8)
for i, (tx, ty) in enumerate(tiles):
    p = assign[i]
    pal = pals[p]
    opts = [SHARED] + pal
    cache = {}
    for yy in range(8):
        for xx in range(8):
            y, x = ty * 8 + yy, tx * 8 + xx
            c = int(img[y, x])
            if c not in cache:
                if c == SHARED:
                    cache[c] = 0
                elif c in pal:
                    cache[c] = pal.index(c) + 1
                else:
                    cache[c] = min(range(len(opts)), key=lambda k: cdist(c, opts[k]))
            out[y, x] = p * 16 + cache[c]

# Palette: exactly 8 x 16 = 128 entries (CGRAM BG palettes 0-7)
flat = []
for p in range(NPAL):
    entries = [GP[SHARED]] + [GP[c] for c in pals[p]]
    entries += [GP[SHARED]] * (16 - len(entries))
    for e in entries:
        flat.extend(e)
assert len(flat) == NPAL * 16 * 3

# ---------------- Verification ----------------
pal_rgb = np.array(flat, dtype=int).reshape(-1, 3)
for v in pal_rgb.flat:  # every color must be an exact BGR555 value
    assert expand(int(v) >> 3) == v
uniq = set()
for ty in range(TY):
    for tx in range(TX):
        blk = out[ty * 8:ty * 8 + 8, tx * 8:tx * 8 + 8]
        assert len(set((blk >> 4).flat)) == 1  # one palette per tile
        b = blk & 15
        vs = [b, b[:, ::-1], b[::-1, :], b[::-1, ::-1]]
        uniq.add(min(v.tobytes() for v in vs))
assert len(uniq) <= 1024
assert int(out.max()) < NPAL * 16

# RGB-level check: <=16 colors per tile, palettes fit 8 sets
rgb = pal_rgb[out]
for ty in range(TY):
    for tx in range(TX):
        blk = rgb[ty * 8:ty * 8 + 8, tx * 8:tx * 8 + 8].reshape(-1, 3)
        assert len({tuple(c) for c in blk}) <= 16

im = Image.fromarray(out, mode="P")
im.putpalette(flat)
im.save("art.png")
print("palettes used:", sum(1 for p in pals if p), "unique tiles:", len(uniq),
      "colors per palette:", [len(p) for p in pals])
