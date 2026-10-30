from PIL import Image

# One NES sprite palette: 3 colors + transparent (NES system palette values)
PAL = {
    '.': (0, 0, 0, 0),
    'K': (0, 0, 0, 255),         # outline / shield edge / dark
    'S': (188, 188, 188, 255),   # steel
    'B': (0, 88, 248, 255),      # blue: tunic, plume, boots
}

# Map original symbols onto the reduced palette
REMAP = {'.': '.', 'K': 'K', 'S': 'S', 'B': 'B', 'D': 'K', 'R': 'B', 'N': 'B'}

UPPER = [
    "................",
    ".......RR.......",
    "......KRRK......",
    ".....KSSSSK.....",
    ".....KSSKKSK....",
    ".....KSSSSSK....",
    "......KKKKK.....",
    "....KBBBBBBK....",
    "...KDKBBBBBSK...",
    "...KDKBBBBBSK...",
    "....KBBBBBBK....",
]

LEGS = {
    0: [
        "....KSSKKSSK....",
        "...KSSK..KSSK...",
        "..KSSK....KSSK..",
        "..KNNK....KNNNK.",
        "..KKK.....KKKK..",
    ],
    1: [
        ".....KSSKSSK....",
        ".....KSSKSSK....",
        ".....KSSKSSK....",
        ".....KNNKNNK....",
        "......KKKKKK....",
    ],
    2: [
        "....KSSKKSSK....",
        ".....KSKKSK.....",
        "....KSK..KSK....",
        "....KNK..KNNK...",
        "....KKK..KKKK...",
    ],
    3: [
        ".....KSSKSSK....",
        ".....KSSKSSK....",
        ".....KSSKSSK....",
        ".....KNNKNNK....",
        "......KKKKKK....",
    ],
}

BOB = {0: 0, 1: -1, 2: 0, 3: -1}

sheet = Image.new("RGBA", (64, 16), (0, 0, 0, 0))
px = sheet.load()

for f in range(4):
    ox = f * 16
    dy = BOB[f]
    for y, row in enumerate(UPPER):
        row = row.ljust(16, '.')
        assert len(row) == 16
        for x, c in enumerate(row):
            c = REMAP[c]
            if c != '.':
                yy = y + dy
                if 0 <= yy < 16:
                    px[ox + x, yy] = PAL[c]
    for i, row in enumerate(LEGS[f]):
        row = row.ljust(16, '.')
        assert len(row) == 16
        for x, c in enumerate(row):
            c = REMAP[c]
            if c != '.':
                px[ox + x, 11 + i] = PAL[c]

sheet.save("art.png")
