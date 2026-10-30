from PIL import Image

# NES hardware palette entries (common 2C02 RGB approximations)
NES = {
    "K": (0, 0, 0),         # $0F black (outline)
    "G": (188, 188, 188),   # $10 light gray (armor)
    "R": (248, 56, 0),      # $16 red (plume / tunic / far leg)
}
# One sprite sub-palette: [transparent, $0F, $10, $16] -> 3 colors + transparency.

TOP = [
    "......RRR.......",
    ".....RRKKKK.....",
    "....KKGGGGGK....",
    "....KGGGGGGGK...",
    "....KGGGKKKKK...",
    "....KGGGGGGGK...",
    ".....KGGGGGK....",
    "....KKKKKKKKK...",
    "...KGGRRRRRGK...",
    "...KGGRRRRRKGK..",
    "....KKRRRRRKKK..",
]

LEGS = [
    [  # frame 0: contact, near leg forward
        "....KGGGGGGK....",
        "...KRRK.KGGK....",
        "..KRRK...KGGK...",
        ".KRRK.....KGGK..",
        ".KKKK.....KKKKK.",
    ],
    [  # frame 1: passing
        "....KGGGGGGK....",
        ".....KGGRRK.....",
        ".....KGGRRK.....",
        ".....KGGKRRK....",
        "....KKKKKKKKK...",
    ],
    [  # frame 2: contact, far leg forward
        "....KGGGGGGK....",
        "...KGGK.KRRK....",
        "..KGGK...KRRK...",
        ".KGGK.....KRRK..",
        ".KKKK.....KKKKK.",
    ],
    [  # frame 3: passing, far leg lifted
        "....KGGGGGGK....",
        ".....KGGGGK.....",
        ".....KRRGGGK....",
        "....KRRK.KGGK...",
        "....KKK...KKKKK.",
    ],
]

img = Image.new("RGBA", (64, 16), (0, 0, 0, 0))
px = img.load()

for f, legs in enumerate(LEGS):
    rows = TOP + legs
    assert len(rows) == 16
    for y, row in enumerate(rows):
        assert len(row) == 16, (f, y, row)
        for x, c in enumerate(row):
            if c != ".":
                r, g, b = NES[c]
                px[f * 16 + x, y] = (r, g, b, 255)

# Validate NES sprite constraints: each 8x8 tile <= 3 opaque colors,
# whole sheet fits in a single sprite sub-palette.
all_colors = set()
for ty in range(0, 16, 8):
    for tx in range(0, 64, 8):
        cols = set()
        for y in range(ty, ty + 8):
            for x in range(tx, tx + 8):
                p = px[x, y]
                if p[3]:
                    cols.add(p[:3])
        assert len(cols) <= 3, (tx, ty, cols)
        all_colors |= cols
assert len(all_colors) <= 3

img.save("art.png")
