import random
from PIL import Image, ImageDraw

W, H = 256, 224
random.seed(7)

def q(c):  # snap to SNES 15-bit RGB (5 bits per channel)
    return tuple((v >> 3) * 8 for v in c)

# shared stone shades (index 0 of every palette is s0)
S = [q(c) for c in [(0x10,0x10,0x18),(0x28,0x28,0x38),(0x40,0x40,0x58),(0x58,0x58,0x70),
                    (0x70,0x70,0x88),(0x90,0x90,0xa8),(0xb0,0xb0,0xc0)]]
F = [q(c) for c in [(0x38,0x30,0x40),(0x50,0x48,0x58),(0x68,0x60,0x70),(0x80,0x78,0x88)]]
MOSS = [q((0x30,0x50,0x30)), q((0x48,0x70,0x40))]
WOOD = [q(c) for c in [(0x20,0x10,0x08),(0x48,0x28,0x10),(0x68,0x40,0x18),(0x88,0x58,0x28)]]
GOLD = q((0xe0,0xb0,0x30))
FIRE = [q(c) for c in [(0xc8,0x28,0x10),(0xf0,0x78,0x18),(0xf8,0xd0,0x38),(0xff,0xf0,0xb0)]]
GLOW = [q((0x80,0x60,0x58)), q((0xa0,0x78,0x60))]

PAL = [
    S + F + MOSS,                       # P0: stone walls + floor
    S + WOOD + [GOLD],                  # P1: door
    S + FIRE + [WOOD[1], WOOD[2]] + GLOW,  # P2: torches
]
for p in PAL:
    assert len(p) <= 16 and p[0] == S[0]

img = Image.new("RGB", (W, H), S[0])
d = ImageDraw.Draw(img)
px = img.load()

# ---- floor (flagstones 16x16, offset rows) ----
for ty, y in enumerate(range(48, 208, 16)):
    off = 8 if ty % 2 else 0
    for x in range(16 - off, 240, 16):
        x0, y0 = x, y
        base = random.choice([F[1], F[2], F[2], F[1]])
        d.rectangle([x0, y0, x0+15, y0+15], fill=S[0])
        d.rectangle([x0+1, y0+1, x0+14, y0+14], fill=base)
        d.line([x0+1, y0+1, x0+14, y0+1], fill=F[3])
        d.line([x0+1, y0+1, x0+1, y0+14], fill=F[3])
        d.line([x0+2, y0+14, x0+14, y0+14], fill=F[0])
        d.line([x0+14, y0+2, x0+14, y0+14], fill=F[0])
        for _ in range(5):
            sx, sy = x0+random.randint(3, 12), y0+random.randint(3, 12)
            px[sx, sy] = random.choice([F[0], F[3]])
        if random.random() < 0.12:
            mx, my = x0+random.randint(3, 10), y0+random.randint(3, 10)
            for k in range(3):
                px[mx+k, my] = MOSS[random.randint(0, 1)]
                px[mx+k, my+1] = MOSS[0]
# clip floor outside area (flagstones overhang at edges) is covered by walls below

# ---- walls ----
def top_block(x0, y0, x1, y1):
    d.rectangle([x0, y0, x1, y1], fill=S[4])

# top wall face (y 0..47) bricks
d.rectangle([0, 0, 255, 47], fill=S[0])
for r, y in enumerate(range(0, 40, 8)):
    off = 8 if r % 2 else 0
    for x in range(-off, 256, 16):
        c = random.choice([S[2], S[3], S[3], S[3]])
        xa, xb = max(x+1, 0), min(x+15, 255)
        d.rectangle([xa, y+1, xb, y+7], fill=c)
        d.line([xa, y+1, xb, y+1], fill=S[4])
        d.line([xa, y+7, xb, y+7], fill=S[1])
        if random.random() < 0.5:
            px[min(max(x+random.randint(3, 12), 0), 255), y+random.randint(3, 5)] = S[1]
# ledge at base of face
d.rectangle([0, 40, 255, 47], fill=S[0])
d.line([0, 40, 255, 40], fill=S[6])
d.rectangle([0, 41, 255, 43], fill=S[5])
d.rectangle([0, 44, 255, 45], fill=S[3])
d.line([0, 46, 255, 46], fill=S[1])
d.line([0, 47, 255, 47], fill=S[0])

# side and bottom wall tops (blocks)
def block_wall(x0, y0, x1, y1, vertical):
    d.rectangle([x0, y0, x1, y1], fill=S[0])
    if vertical:
        for y in range(y0, y1+1, 16):
            c = random.choice([S[3], S[4]])
            d.rectangle([x0+1, y+1, x1-1, min(y+14, y1)], fill=c)
            d.line([x0+1, y+1, x1-1, y+1], fill=S[5])
            d.line([x0+1, y+1, x0+1, min(y+14, y1)], fill=S[5])
            d.line([x1-1, y+2, x1-1, min(y+14, y1)], fill=S[2])
    else:
        for x in range(x0, x1+1, 16):
            c = random.choice([S[3], S[4]])
            d.rectangle([x+1, y0+1, min(x+14, x1), y1-1], fill=c)
            d.line([x+1, y0+1, min(x+14, x1), y0+1], fill=S[5])
            d.line([x+1, y0+1, x+1, y1-1], fill=S[5])
            d.line([x+2, y1-1, min(x+14, x1), y1-1], fill=S[2])
block_wall(0, 48, 15, 207, True)
block_wall(240, 48, 255, 207, True)
block_wall(0, 208, 255, 223, False)
# inner edge shading of side walls onto floor
d.line([16, 48, 16, 207], fill=S[1])
d.line([239, 48, 239, 207], fill=S[1])
d.line([16, 207, 239, 207], fill=S[1])
# shadow under top wall (dither)
for y in range(48, 56):
    for x in range(16, 240):
        lim = 56 - y
        if (x + y) % 2 == 0 and lim > 3 or (y < 51 and (x + y) % 2 == 1):
            px[x, y] = F[0]
for y in range(48, 207):
    for x in (17, 18, 237, 238):
        if (x + y) % 2 == 0:
            px[x, y] = F[0]

# ---- torches ----
TORCH_CX = [76, 180]
TORCH_RECTS = []
for cx in TORCH_CX:
    rx0 = cx - 20
    TORCH_RECTS.append((rx0, 0, rx0 + 39, 47))
    fy = 24
    # glow on bricks (inside rect only)
    for y in range(0, 40):
        for x in range(rx0, rx0 + 40):
            dist = ((x - cx) ** 2 + ((y - fy) * 1.2) ** 2) ** 0.5
            c = px[x, y]
            if dist < 15 or (dist < 20 and (x + y) % 2 == 0):
                if c == S[3]: px[x, y] = GLOW[0]
                elif c == S[4]: px[x, y] = GLOW[1]
    # bracket
    d.rectangle([cx-1, 30, cx+1, 40], fill=WOOD[1])
    d.line([cx-1, 30, cx-1, 40], fill=WOOD[2])
    d.rectangle([cx-4, 28, cx+4, 30], fill=S[2])
    d.line([cx-4, 28, cx+4, 28], fill=S[5])
    d.rectangle([cx-3, 31, cx+3, 31], fill=S[1])
    d.rectangle([cx-2, 40, cx+2, 41], fill=S[1])
    # flame
    d.polygon([(cx, 10), (cx+5, 19), (cx+5, 25), (cx, 28), (cx-5, 25), (cx-5, 19)], fill=FIRE[0])
    d.polygon([(cx, 13), (cx+4, 20), (cx+4, 25), (cx, 27), (cx-4, 25), (cx-4, 20)], fill=FIRE[1])
    d.polygon([(cx, 17), (cx+2, 22), (cx+2, 26), (cx, 27), (cx-2, 26), (cx-2, 22)], fill=FIRE[2])
    d.rectangle([cx-1, 23, cx, 26], fill=FIRE[3])

# ---- door ----
DX0, DX1, DY0, DY1 = 112, 143, 16, 47
d.rectangle([DX0, DY0, DX1, DY1], fill=S[0])
d.rectangle([DX0+1, DY0+1, DX1-1, DY1], fill=S[2])
d.line([DX0+1, DY0+1, DX1-1, DY0+1], fill=S[5])
d.rectangle([DX0+3, DY0+3, DX1-3, DY1-4], fill=WOOD[0])
for i in range(4):
    x0 = DX0 + 4 + i * 6
    d.rectangle([x0, DY0+4, x0+4, DY1-4], fill=WOOD[2])
    d.line([x0, DY0+4, x0, DY1-4], fill=WOOD[3])
    d.line([x0+4, DY0+4, x0+4, DY1-4], fill=WOOD[1])
    for y in range(DY0+6, DY1-4, 5):
        if random.random() < 0.5:
            px[x0+2, y] = WOOD[1]
for y in (DY0+8, DY1-12):
    d.rectangle([DX0+3, y, DX1-3, y+2], fill=S[2])
    d.line([DX0+3, y, DX1-3, y], fill=S[4])
    d.line([DX0+3, y+2, DX1-3, y+2], fill=S[1])
    for x in (DX0+6, DX0+12, DX0+19, DX0+25):
        px[x, y+1] = S[6]
# handle
d.rectangle([DX0+14, DY0+17, DX0+17, DY0+20], outline=GOLD)
px[DX0+15, DY0+21] = GOLD
# threshold
d.rectangle([DX0-1, DY1-3, DX1+1, DY1-2], fill=S[5])
d.line([DX0-1, DY1-1, DX1+1, DY1-1], fill=S[3])
d.line([DX0-1, DY1, DX1+1, DY1], fill=S[1])

# ---- palette assignment per 8x8 tile & validation ----
def pal_for(tx, ty):
    x0, y0 = tx*8, ty*8
    if DX0 <= x0 and x0+7 <= DX1 and DY0 <= y0 and y0+7 <= DY1 + 0:
        return 1
    for (a, b, c, e) in TORCH_RECTS:
        if a <= x0 and x0+7 <= c and b <= y0 and y0+7 <= e:
            return 2
    return 0

# door threshold sticks 1px outside door tiles; keep those pixels stone-only (they are)
tiles = set()
used = [0, 0, 0]
for ty in range(H // 8):
    for tx in range(W // 8):
        cols = set()
        for y in range(8):
            for x in range(8):
                cols.add(px[tx*8+x, ty*8+y])
        p = pal_for(tx, ty)
        if not cols <= set(PAL[p]):
            for alt in range(3):
                if cols <= set(PAL[alt]):
                    p = alt
                    break
            else:
                raise SystemExit(f"tile {tx},{ty} has colors outside any palette: {cols - set(PAL[p])}")
        used[p] += 1
        tiles.add((p, tuple(px[tx*8+x, ty*8+y] for y in range(8) for x in range(8))))

assert len(tiles) <= 1024
img.save("art.png")
