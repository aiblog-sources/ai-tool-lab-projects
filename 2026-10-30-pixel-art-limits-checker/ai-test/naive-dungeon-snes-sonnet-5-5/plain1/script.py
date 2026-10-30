from PIL import Image, ImageDraw, ImageChops
import random

W, H = 256, 224
random.seed(7)
img = Image.new("RGB", (W, H), (20, 16, 24))
d = ImageDraw.Draw(img)

# Limited palette (SNES-like)
FLOOR = [(92, 86, 98), (106, 100, 112), (78, 72, 86)]
FLOOR_LINE = (56, 50, 64)
WALL = [(70, 62, 80), (84, 76, 94), (54, 48, 64)]
WALL_DARK = (36, 30, 46)
WALL_LIGHT = (112, 104, 124)
WOOD = [(120, 76, 40), (96, 58, 30), (72, 42, 22)]
IRON = (50, 50, 58)

WALL_TOP = 64   # north wall height
SIDE = 24       # side wall width
BOT = 24        # bottom wall height

# Floor tiles
ts = 16
for y in range(WALL_TOP, H - BOT, ts):
    for x in range(SIDE, W - SIDE, ts):
        c = random.choice(FLOOR)
        d.rectangle([x, y, x + ts - 1, y + ts - 1], fill=c)
        d.line([x, y, x + ts - 1, y], fill=FLOOR_LINE)
        d.line([x, y, x, y + ts - 1], fill=FLOOR_LINE)
        d.point((x + 1, y + 1), fill=FLOOR[1])
        for _ in range(3):
            px, py = x + random.randint(2, 13), y + random.randint(2, 13)
            d.point((px, py), fill=random.choice([FLOOR[2], FLOOR[1]]))

def brick_area(x0, y0, x1, y1, bw=16, bh=8):
    d.rectangle([x0, y0, x1, y1], fill=WALL[0])
    row = 0
    for y in range(y0, y1 + 1, bh):
        off = (bw // 2) if row % 2 else 0
        for x in range(x0 - off, x1 + 1, bw):
            bx0, bx1 = max(x, x0), min(x + bw - 1, x1)
            by1 = min(y + bh - 1, y1)
            if bx0 > bx1:
                continue
            d.rectangle([bx0, y, bx1, by1], fill=random.choice(WALL))
            d.line([bx0, y, bx1, y], fill=WALL_LIGHT)
            d.line([bx0, by1, bx1, by1], fill=WALL_DARK)
            if x >= x0:
                d.line([x, y, x, by1], fill=WALL_DARK)
        row += 1

# Walls
brick_area(0, 0, W - 1, WALL_TOP - 1)
brick_area(0, WALL_TOP, SIDE - 1, H - 1)
brick_area(W - SIDE, WALL_TOP, W - 1, H - 1)
brick_area(SIDE, H - BOT, W - SIDE - 1, H - 1)

# Wall cap / shadow on floor
d.line([SIDE, WALL_TOP - 1, W - SIDE - 1, WALL_TOP - 1], fill=WALL_LIGHT)
for i in range(4):
    sh = (70 - i * 4, 64 - i * 4, 78 - i * 4)
    d.line([SIDE, WALL_TOP + i, W - SIDE - 1, WALL_TOP + i], fill=sh)
for i in range(3):
    d.line([SIDE + i, WALL_TOP, SIDE + i, H - BOT - 1], fill=(64 - i * 4, 58 - i * 4, 72 - i * 4))
    d.line([W - SIDE - 1 - i, WALL_TOP, W - SIDE - 1 - i, H - BOT - 1], fill=(64 - i * 4, 58 - i * 4, 72 - i * 4))

# Door in north wall
dw, dh = 40, 52
dx = (W - dw) // 2
dy = WALL_TOP - dh
d.rectangle([dx - 4, dy - 4, dx + dw + 3, WALL_TOP - 1], fill=WALL_DARK)  # frame
d.rectangle([dx - 2, dy - 2, dx + dw + 1, WALL_TOP - 1], fill=(100, 92, 112))
d.rectangle([dx, dy, dx + dw - 1, WALL_TOP - 1], fill=WOOD[1])
d.rectangle([dx, dy, dx + dw - 1, dy + 1], fill=WOOD[2])
for px in range(dx, dx + dw, 8):
    d.rectangle([px, dy + 2, px + 6, WALL_TOP - 1], fill=WOOD[0])
    d.line([px + 7, dy + 2, px + 7, WALL_TOP - 1], fill=WOOD[2])
    d.line([px + 1, dy + 2, px + 1, WALL_TOP - 1], fill=(140, 92, 52))
for by in (dy + 10, dy + 36):
    d.rectangle([dx, by, dx + dw - 1, by + 3], fill=IRON)
    d.line([dx, by, dx + dw - 1, by], fill=(84, 84, 96))
    for rx in range(dx + 3, dx + dw, 12):
        d.point((rx, by + 1), fill=(150, 150, 160))
d.rectangle([dx + dw - 9, dy + 26, dx + dw - 6, dy + 29], fill=(210, 170, 60))
d.point((dx + dw - 9, dy + 26), fill=(255, 230, 130))
d.rectangle([dx - 4, WALL_TOP, dx + dw + 3, WALL_TOP + 2], fill=(120, 112, 130))

def torch(cx, cy):
    global img
    glow = Image.new("RGB", (W, H), (0, 0, 0))
    gd = ImageDraw.Draw(glow)
    for r, col in ((34, (14, 8, 0)), (26, (24, 14, 2)), (18, (36, 20, 4)), (10, (50, 28, 6))):
        gd.ellipse([cx - r, cy - r, cx + r, cy + r], fill=col)
    out = ImageChops.add(img, glow)
    img.paste(out)
    dd = ImageDraw.Draw(img)
    dd.rectangle([cx - 3, cy + 6, cx + 2, cy + 8], fill=IRON)
    dd.rectangle([cx - 2, cy, cx + 1, cy + 10], fill=WOOD[1])
    dd.line([cx - 2, cy, cx - 2, cy + 10], fill=WOOD[0])
    dd.rectangle([cx - 3, cy - 2, cx + 2, cy + 1], fill=IRON)
    dd.polygon([(cx - 3, cy - 2), (cx - 1, cy - 8), (cx, cy - 13), (cx + 1, cy - 8), (cx + 3, cy - 2)], fill=(220, 90, 20))
    dd.polygon([(cx - 2, cy - 2), (cx, cy - 10), (cx + 2, cy - 2)], fill=(250, 170, 40))
    dd.polygon([(cx - 1, cy - 2), (cx, cy - 7), (cx + 1, cy - 2)], fill=(255, 240, 150))

torch(68, 36)
torch(188, 36)
d = ImageDraw.Draw(img)

# Floor light pools from torches (subtle dither)
for cx in (68, 188):
    for y in range(WALL_TOP, WALL_TOP + 40):
        for x in range(cx - 36, cx + 37):
            if SIDE <= x < W - SIDE:
                dist = ((x - cx) ** 2 + (y - 30) ** 2) ** 0.5
                if dist < 62 and (x + y) % 2 == 0 and dist > 30:
                    if random.random() < (62 - dist) / 62:
                        r, g, b = img.getpixel((x, y))
                        img.putpixel((x, y), (min(255, r + 18), min(255, g + 10), b))

d = ImageDraw.Draw(img)
for cx, cy in ((90, 150), (170, 120), (60, 110)):
    pts = [(cx, cy)]
    for _ in range(5):
        cx += random.randint(-4, 5)
        cy += random.randint(1, 5)
        pts.append((cx, cy))
    d.line(pts, fill=FLOOR_LINE)

# ---------------------------------------------------------------
# SNES conversion: RGB555, 8x8 tiles, max 8 palettes of 16 colors
# (index 0 = shared backdrop colour, so 15 usable colours/palette)
# ---------------------------------------------------------------
def snap(c):
    return tuple(((v >> 3) << 3) | (v >> 5) for v in c[:3])

def dist(a, b):
    return sum((a[i] - b[i]) ** 2 for i in range(3))

def nearest(c, cols):
    return min(cols, key=lambda k: dist(c, k))

BACKDROP = snap((20, 16, 24))
TILE = 8
MAXC = 15
NPAL = 8

def tile_pixels(im, tx, ty):
    return [im.getpixel((tx * TILE + i, ty * TILE + j))
            for j in range(TILE) for i in range(TILE)]

def reduce_tile(pix):
    """Limit a tile to MAXC non-backdrop colours."""
    cnt = {}
    for p in pix:
        cnt[p] = cnt.get(p, 0) + 1
    mp = {c: c for c in cnt}
    while len([c for c in cnt if c != BACKDROP]) > MAXC:
        cand = [c for c in cnt if c != BACKDROP]
        least = min(cand, key=lambda k: cnt[k])
        others = [c for c in cnt if c != least]
        tgt = nearest(least, others)
        cnt[tgt] += cnt[least]
        del cnt[least]
        for k, v in mp.items():
            if v == least:
                mp[k] = tgt
    return [mp[p] for p in pix]

def build(ncol):
    q = img.quantize(ncol, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE).convert("RGB")
    q = Image.eval(q, lambda v: v)  # keep as RGB
    q = Image.frombytes("RGB", q.size, b"".join(bytes(snap(p)) for p in q.getdata()))
    tiles = {}
    for ty in range(H // TILE):
        for tx in range(W // TILE):
            tiles[(tx, ty)] = reduce_tile(tile_pixels(q, tx, ty))
    return tiles

def pack(tiles, force=False):
    pals = []
    assign = {}
    order = sorted(tiles, key=lambda k: -len(set(tiles[k]) - {BACKDROP}))
    for k in order:
        T = set(tiles[k]) - {BACKDROP}
        best, bcost = None, None
        for i, P in enumerate(pals):
            add = len(T - P)
            if len(P) + add <= MAXC and (bcost is None or add < bcost):
                best, bcost = i, add
        if best is None and len(pals) < NPAL:
            pals.append(set())
            best = len(pals) - 1
        if best is None:
            if not force:
                return None
            # fallback: palette needing the least remap error
            def err(i):
                P = pals[i] | {BACKDROP}
                return sum(dist(c, nearest(c, P)) for c in T)
            best = min(range(len(pals)), key=err)
            P = pals[best] | {BACKDROP}
            tiles[k] = [nearest(p, P) for p in tiles[k]]
            assign[k] = best
            continue
        pals[best] |= T
        assign[k] = best
    return pals, assign

result = None
for n in range(120, 7, -4):
    tiles = build(n)
    r = pack(tiles)
    if r:
        result = (tiles, r)
        break
if result is None:
    tiles = build(48)
    r = pack(tiles, force=True)
    result = (tiles, r)

tiles, (pals, assign) = result
out = Image.new("RGB", (W, H))
for (tx, ty), pix in tiles.items():
    t = Image.new("RGB", (TILE, TILE))
    t.putdata(pix)
    out.paste(t, (tx * TILE, ty * TILE))

out.save("art.png")
