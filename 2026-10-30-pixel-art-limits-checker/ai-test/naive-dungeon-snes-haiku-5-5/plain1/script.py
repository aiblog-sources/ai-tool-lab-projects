from PIL import Image, ImageDraw
import random
import numpy as np

W, H = 256, 224
OUT = "art.png"
rng = random.Random(42)


def hx(h):
    return tuple(int(h[i:i + 2], 16) for i in (0, 2, 4))


def mix(a, b, t):
    return tuple(int(a[i] + (b[i] - a[i]) * t) for i in range(3))


# Palette
FLOOR_A = [hx("5c5c66"), hx("575761"), hx("62626c")]
FLOOR_HI = hx("72727c")
FLOOR_LINE = hx("383842")
WALL_A = [hx("7d6b57"), hx("72614f"), hx("87765f")]
WALL_HI = hx("9c8b72")
WALL_LINE = hx("3b3026")
WOOD_A = [hx("8b5a2b"), hx("7a4d24"), hx("95633a")]
WOOD_LINE = hx("4a2c14")
IRON = hx("3a3a40")
IRON_HI = hx("6a6a74")
FLAME = [hx("ffe36b"), hx("ffb02e"), hx("ff6a1a"), hx("d7361c")]
TORCH_WOOD = hx("6b4423")
BRACKET = hx("55555e")
GLOW = hx("ffa046")

# Layout: floor area is 224x176 (14x11 tiles of 16px)
FX0, FY0, FX1, FY1 = 16, 32, 240, 208

img = Image.new("RGB", (W, H), FLOOR_LINE)
d = ImageDraw.Draw(img)


def stone_block(x0, y0, x1, y1, palette, line, hi, sw, sh):
    """Staggered stone pattern with 1px mortar lines."""
    d.rectangle([x0, y0, x1 - 1, y1 - 1], fill=line)
    row = 0
    y = y0
    while y < y1:
        off = (sw // 2) if row % 2 else 0
        x = x0 - off
        while x < x1:
            l = max(x, x0)
            r = min(x + sw - 2, x1 - 1)
            t = y
            b = min(y + sh - 2, y1 - 1)
            if r >= l and b >= t:
                d.rectangle([l, t, r, b], fill=rng.choice(palette))
                d.line([(l, t), (r, t)], fill=hi)
            x += sw
        y += sh
        row += 1


# Floor
stone_block(FX0, FY0, FX1, FY1, FLOOR_A, FLOOR_LINE, FLOOR_HI, 16, 8)

# Walls (top is thicker: two tiles)
stone_block(0, 0, W, FY0, WALL_A, WALL_LINE, WALL_HI, 32, 8)
stone_block(0, FY0, FX0, FY1, WALL_A, WALL_LINE, WALL_HI, 16, 8)
stone_block(FX1, FY0, W, FY1, WALL_A, WALL_LINE, WALL_HI, 16, 8)
stone_block(0, FY1, W, H, WALL_A, WALL_LINE, WALL_HI, 32, 8)

# Wooden door centred in the top wall
DX0, DX1 = 112, 144
d.rectangle([DX0 - 2, 0, DX1 + 1, FY0 - 1], fill=WALL_LINE)  # frame
for x in range(DX0, DX1, 4):
    d.rectangle([x, 0, x + 2, FY0 - 1], fill=rng.choice(WOOD_A))
    d.line([(x + 3, 0), (x + 3, FY0 - 1)], fill=WOOD_LINE)
for by in (5, 24):  # iron bands
    d.rectangle([DX0, by, DX1 - 1, by + 1], fill=IRON)
    for sx in (DX0 + 3, DX0 + 15, DX0 + 27):
        d.point([(sx, by), (sx + 1, by)], fill=IRON_HI)
d.ellipse([136, 14, 139, 17], fill=IRON_HI, outline=IRON)  # handle

# Torches on the top wall, flanking the door
TORCHES = [60, 196]
for cx in TORCHES:
    d.rectangle([cx - 3, 22, cx + 3, 24], fill=BRACKET)       # bracket
    d.rectangle([cx - 1, 14, cx + 1, 22], fill=TORCH_WOOD)    # stick
    d.ellipse([cx - 3, 6, cx + 3, 14], fill=FLAME[3])         # outer flame
    d.ellipse([cx - 2, 7, cx + 2, 13], fill=FLAME[2])
    d.ellipse([cx - 1, 8, cx + 1, 12], fill=FLAME[1])
    d.point([(cx, 9), (cx, 10)], fill=FLAME[0])               # core

px = img.load()

# Shadow cast by the top and left walls onto the floor
for x in range(FX0, FX1):
    for i in range(4):
        y = FY0 + i
        px[x, y] = mix(px[x, y], (0, 0, 0), 0.30 - i * 0.06)
for y in range(FY0, FY1):
    for i in range(3):
        x = FX0 + i
        px[x, y] = mix(px[x, y], (0, 0, 0), 0.25 - i * 0.07)

# Floor grime specks
for _ in range(260):
    x = rng.randrange(FX0 + 3, FX1)
    y = rng.randrange(FY0 + 4, FY1)
    if rng.random() < 0.5:
        px[x, y] = mix(px[x, y], (0, 0, 0), 0.25)
    else:
        px[x, y] = mix(px[x, y], (255, 255, 255), 0.08)

# Warm torch light falloff across the whole screen
for cx in TORCHES:
    ox, oy, R = cx, 10, 60
    for y in range(H):
        for x in range(W):
            dist = ((x - ox) ** 2 + (y - oy) ** 2) ** 0.5
            if dist < R:
                t = 1 - dist / R
                px[x, y] = mix(px[x, y], GLOW, 0.22 * t * t)

# ---------------------------------------------------------------------------
# SNES conversion
#   * Colors: 15-bit RGB, 5 bits per channel.
#   * Palettes: 8 BG palettes, 16 entries each. Entry 0 is transparent
#     (backdrop), so each palette holds at most 15 usable colors.
#   * Tiles: 8x8 pixels, each tile uses exactly one palette.
# ---------------------------------------------------------------------------

NUM_PALETTES = 8
COLORS_PER_PALETTE = 15  # entry 0 is transparent on SNES BG


def snap(a):
    """Map 0-255 channel values onto the 5-bit SNES grid (expanded to 8 bits)."""
    v = np.clip(np.round(np.asarray(a, dtype=np.float64) * 31.0 / 255.0), 0, 31)
    v = v.astype(np.int32)
    return ((v << 3) | (v >> 2)).astype(np.uint8)


arr = np.asarray(img, dtype=np.uint8)
S = snap(arr)                                  # 15-bit snapped source
snapped_img = Image.fromarray(S, "RGB")

# Pick a candidate colour set (up to 8 * 15) from the snapped image
q = snapped_img.quantize(
    colors=NUM_PALETTES * COLORS_PER_PALETTE,
    method=Image.Quantize.MEDIANCUT,
    dither=Image.Dither.NONE,
)
used = np.unique(np.asarray(q))
raw = np.array(q.getpalette(), dtype=np.float64).reshape(-1, 3)[used]
cols = np.unique(snap(raw), axis=0)            # snap palette entries to 5-bit

# Sort by luminance and split into palettes of at most 15 colours each
lum = cols.astype(np.float64) @ np.array([0.3, 0.59, 0.11])
cols = cols[np.argsort(lum)]
groups = [cols[i:i + COLORS_PER_PALETTE]
          for i in range(0, len(cols), COLORS_PER_PALETTE)][:NUM_PALETTES]

# Assign each 8x8 tile to the palette that represents it with least error
out = np.zeros((H, W, 3), dtype=np.uint8)
for ty in range(H // 8):
    for tx in range(W // 8):
        y0, x0 = ty * 8, tx * 8
        T = S[y0:y0 + 8, x0:x0 + 8].astype(np.int32)
        best_err = None
        best_d = None
        best_g = None
        for g in groups:
            G = g.astype(np.int32)
            dist = ((T[:, :, None, :] - G[None, None, :, :]) ** 2).sum(-1)
            err = dist.min(-1).sum()
            if best_err is None or err < best_err:
                best_err, best_d, best_g = err, dist, g
        pick = best_d.argmin(-1)
        out[y0:y0 + 8, x0:x0 + 8] = best_g[pick]

Image.fromarray(out, "RGB").save(OUT)
