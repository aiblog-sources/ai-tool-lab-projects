from PIL import Image

# Palette: '.' is transparent
PALETTE = {
    'K': (26, 28, 44, 255),     # outline
    'S': (200, 210, 220, 255),  # steel highlight
    'G': (127, 140, 154, 255),  # steel mid
    'D': (74, 85, 104, 255),    # steel shadow
    'R': (178, 34, 34, 255),    # red plume
    'B': (107, 62, 30, 255),    # boots / belt
    'Y': (224, 176, 32, 255),   # gold tabard
}

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

# Leg rows (11-15) for each of the 4 walk frames
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
        ".....KDDKDDK.....",
        ".....KDDKDDK.....",
        ".....KBBKBBK.....",
        "....KBBBKBBBK....",
        "....KKKKKKKKK....",
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
TILE = 8              # NES sprite tile size
MAX_COLORS = 3        # opaque colors allowed per tile (plus transparent)


def color_dist(a, b):
    return sum((a[i] - b[i]) ** 2 for i in range(3))


def build_frame(f):
    rows = HEAD_TORSO + LEGS[f]
    grid = []
    for row in rows:
        row = row.ljust(FRAME_W, '.')[:FRAME_W]
        grid.append(list(row))
    while len(grid) < FRAME_H:
        grid.append(['.'] * FRAME_W)
    return grid


def reduce_tile(grid, x0, y0):
    """Merge colors inside one 8x8 tile until at most MAX_COLORS remain."""
    while True:
        counts = {}
        for y in range(y0, y0 + TILE):
            for x in range(x0, x0 + TILE):
                ch = grid[y][x]
                if ch != '.':
                    counts[ch] = counts.get(ch, 0) + 1
        if len(counts) <= MAX_COLORS:
            return
        # Least-used color is the one to remove (deterministic tie-break)
        victim = min(counts, key=lambda c: (counts[c], c))
        others = [c for c in counts if c != victim]
        # Replace it with the visually closest remaining color in this tile
        target = min(others,
                     key=lambda c: (color_dist(PALETTE[victim], PALETTE[c]), c))
        for y in range(y0, y0 + TILE):
            for x in range(x0, x0 + TILE):
                if grid[y][x] == victim:
                    grid[y][x] = target


img = Image.new("RGBA", (FRAME_W * FRAMES, FRAME_H), (0, 0, 0, 0))

for f in range(FRAMES):
    grid = build_frame(f)

    # Enforce the NES color limit on every 8x8 tile of this frame
    for ty in range(0, FRAME_H, TILE):
        for tx in range(0, FRAME_W, TILE):
            reduce_tile(grid, tx, ty)

    for y in range(FRAME_H):
        for x in range(FRAME_W):
            ch = grid[y][x]
            if ch == '.':
                continue
            img.putpixel((f * FRAME_W + x, y), PALETTE[ch])

img.save("art.png")
