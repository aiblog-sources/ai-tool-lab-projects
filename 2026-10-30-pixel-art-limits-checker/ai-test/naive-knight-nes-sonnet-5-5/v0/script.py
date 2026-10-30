from PIL import Image

PAL = {
    '.': (0, 0, 0, 0),
    'K': (16, 16, 24, 255),      # outline
    'S': (188, 188, 200, 255),   # steel
    'D': (92, 92, 116, 255),     # dark steel / shield edge
    'B': (40, 88, 200, 255),     # blue tunic
    'R': (216, 40, 40, 255),     # plume
    'N': (136, 80, 32, 255),     # boots
}

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
    # upper body (rows 0-10), bobbed
    for y, row in enumerate(UPPER):
        row = row.ljust(16, '.')
        assert len(row) == 16
        for x, c in enumerate(row):
            if c != '.':
                yy = y + dy
                if 0 <= yy < 16:
                    px[ox + x, yy] = PAL[c]
    # legs rows 11-15
    for i, row in enumerate(LEGS[f]):
        row = row.ljust(16, '.')
        assert len(row) == 16
        for x, c in enumerate(row):
            if c != '.':
                px[ox + x, 11 + i] = PAL[c]

sheet.save("art.png")
