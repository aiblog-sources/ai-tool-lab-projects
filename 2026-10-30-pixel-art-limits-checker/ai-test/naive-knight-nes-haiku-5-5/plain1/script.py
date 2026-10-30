from PIL import Image

# NES master palette (64 entries, 0xRRGGBB). The NES can only show these colours.
NES_MASTER = [
    0x666666, 0x002A88, 0x1412A7, 0x3B00A4, 0x5C007E, 0x6E0040, 0x6C0700, 0x561D00,
    0x333500, 0x0B4800, 0x005200, 0x004F08, 0x00404D, 0x000000, 0x000000, 0x000000,
    0xADADAD, 0x155FD9, 0x4240FF, 0x7527FE, 0xA01ACC, 0xB71E7B, 0xB53120, 0x994E00,
    0x6B6D00, 0x388700, 0x0C9300, 0x008F32, 0x007C8D, 0x000000, 0x000000, 0x000000,
    0xFFFFFF, 0x64B0FF, 0x9290FF, 0xC676FF, 0xF26AFF, 0xFF6ECC, 0xFF8170, 0xEA9E22,
    0xBCBE00, 0x88D800, 0x5CE430, 0x45E082, 0x48CDDE, 0x4F4F4F, 0x000000, 0x000000,
    0xFFFEFF, 0xC0DFFF, 0xD3D2FF, 0xE8C8FF, 0xFBC2FF, 0xFFC4EA, 0xFFCCC5, 0xF7D8A5,
    0xE4E594, 0xCFEF96, 0xBDF4AB, 0xB3F3CC, 0xB5EBF2, 0xB8B8B8, 0x000000, 0x000000,
]

def rgb_of(c):
    return ((c >> 16) & 255, (c >> 8) & 255, c & 255)

NES_RGB = [rgb_of(c) for c in NES_MASTER]

def dist(a, b):
    return sum((p - q) ** 2 for p, q in zip(a, b))

def nearest_nes(rgb):
    return min(range(64), key=lambda i: (dist(NES_RGB[i], rgb), i))

# Symbolic palette: '.' is transparent
PALETTE = {
    'K': (26, 28, 44),     # outline
    'S': (200, 210, 220),  # steel highlight
    'G': (127, 140, 154),  # steel mid
    'D': (74, 85, 104),    # steel shadow
    'R': (178, 34, 34),    # red plume
    'B': (107, 62, 30),    # boots / belt
    'Y': (224, 176, 32),   # gold tabard
}
SYM_TO_NES = {k: nearest_nes(v) for k, v in PALETTE.items()}

# Shared head and torso (rows 0-10), facing right
HEAD_TORSO = [
    "...RRKKKK.......",
    "..RRKGGGGK......",
    "....KGGGGKK.....",
    "....KGKKKKKK....",
    "....KDDDDDDK....",
    ".....KKKKKK.....",
    "....KSSSSSSK....",
    "...KSSYYYYSSK...",
    "...KSSYYYYSSKK..",
    "...KDSYYYYSDK...",
    "...KDDBBBBDDK...",
]

# Leg rows (11-15) for each of the 4 walk frames, all 16 pixels wide
LEGS = [
    # Frame 1: front leg forward (right), back leg back (left)
    [
        "....KDDK..KDDK..",
        "....KDDK..KDDK..",
        "....KBBK..KBBK..",
        "...KBBBK..KBBBK.",
        "...KKKKK..KKKKK.",
    ],
    # Frame 2: legs passing under the body
    [
        ".....KDDKDDK....",
        ".....KDDKDDK....",
        ".....KBBKBBK....",
        "....KBBBKBBBK...",
        "....KKKKKKKKK...",
    ],
    # Frame 3: legs swapped, front leg reaching
    [
        "..KDDK...KDDK...",
        "..KDDK...KDDK...",
        "..KBBK...KBBK...",
        "..KBBBK...KBBBK.",
        "..KKKKK...KKKKK.",
    ],
    # Frame 4: passing again, back foot lifted
    [
        "....KDDK..KDDK..",
        "....KDDK..KDDK..",
        "....KBBK..KBBK..",
        "....KBBBK.KBBBK.",
        "....KKKKK.KKKKK.",
    ],
]

FRAME_W, FRAME_H = 16, 16
FRAMES = 4
SPRITE_W = 8          # NES sprites are 8 wide (8x16 mode)
MAX_COLORS = 3        # 4 palette entries per sprite, entry 0 is transparent

def limit_colors(grid, x0):
    """Reduce the 8x16 sprite whose left edge is x0 to at most MAX_COLORS colours."""
    while True:
        cells = {p: c for p, c in grid.items() if x0 <= p[0] < x0 + SPRITE_W}
        colors = sorted(set(cells.values()))
        if len(colors) <= MAX_COLORS:
            return
        counts = {c: 0 for c in colors}
        for c in cells.values():
            counts[c] += 1
        # Merge the rarest colour into the closest other colour in this sprite
        victim = min(colors, key=lambda c: (counts[c], c))
        others = [c for c in colors if c != victim]
        target = min(others, key=lambda c: (dist(NES_RGB[c], NES_RGB[victim]), c))
        for p, c in cells.items():
            if c == victim:
                grid[p] = target

# Build the sheet as {(x, y): nes_palette_index} for opaque pixels only
grid = {}
for f in range(FRAMES):
    rows = HEAD_TORSO + LEGS[f]
    for y, row in enumerate(rows):
        row = row.ljust(FRAME_W, '.')[:FRAME_W]
        for x, ch in enumerate(row):
            if ch != '.':
                grid[(f * FRAME_W + x, y)] = SYM_TO_NES[ch]

# Enforce the per-sprite colour limit for both 8x16 halves of every frame
for f in range(FRAMES):
    for s in range(FRAME_W // SPRITE_W):
        limit_colors(grid, f * FRAME_W + s * SPRITE_W)

img = Image.new("RGBA", (FRAME_W * FRAMES, FRAME_H), (0, 0, 0, 0))
for (x, y), idx in grid.items():
    r, g, b = NES_RGB[idx]
    img.putpixel((x, y), (r, g, b, 255))

img.save("art.png")
