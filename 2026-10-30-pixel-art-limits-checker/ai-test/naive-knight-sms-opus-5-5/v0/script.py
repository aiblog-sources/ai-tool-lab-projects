from PIL import Image

# Sega Master System palette: each channel is one of 0, 85, 170, 255 (2 bits per channel)
PAL = {
    '.': None,              # transparent
    'K': (0, 0, 0),         # outline
    'W': (255, 255, 255),   # armor highlight
    'S': (170, 170, 170),   # steel light
    'D': (85, 85, 85),      # steel dark
    'F': (255, 170, 85),    # skin
    'R': (170, 0, 0),       # plume dark
    'r': (255, 0, 0),       # plume light
    'b': (0, 85, 170),      # tunic
    'Y': (255, 170, 0),     # gold belt
    'B': (85, 0, 0),        # boots
}

# Upper body (rows 0-11), arm row variants for a slight arm swing
HEAD = [
    ".....rr.........",
    "....rRRKKK......",
    "...rR.KSSWK.....",
    "......KSSSSK....",
    ".....KSSSKKKK...",
    ".....KSSKFFFK...",
    ".....KSDSKKKK...",
    ".....KDSSSSK....",
]
TORSO_A = [  # hand forward
    "....KSSbbbSKK...",
    "...KSDbbbbSFK...",
    "...KSDYYYYKKK...",
    "....KbbbbbbK....",
]
TORSO_B = [  # hand pulled back
    "....KSSbbbSK....",
    "...KSDbbbbKFK...",
    "...KSDYYYYYK....",
    "....KbbbbbbK....",
]

LEGS = [
    [  # frame 1: near leg forward
        "....KDK..KSK....",
        "...KDK....KSK...",
        "..KBBK....KBBBK.",
        "..KKK.....KKKKK.",
    ],
    [  # frame 2: passing
        ".....KDSK.......",
        ".....KDSK.......",
        ".....KBBBK......",
        ".....KKKKK......",
    ],
    [  # frame 3: far leg forward
        "....KSK..KDK....",
        "...KSK....KDK...",
        "..KBBK....KBBBK.",
        "..KKK.....KKKKK.",
    ],
    [  # frame 4: passing (other leg in front)
        ".....KSDK.......",
        ".....KSDK.......",
        ".....KBBBK......",
        ".....KKKKK......",
    ],
]

TORSOS = [TORSO_A, TORSO_B, TORSO_B, TORSO_A]

sheet = Image.new("RGBA", (64, 16), (0, 0, 0, 0))
px = sheet.load()

for f in range(4):
    rows = HEAD + TORSOS[f] + LEGS[f]
    assert len(rows) == 16
    for y, row in enumerate(rows):
        row = row.ljust(16, '.')[:16]
        for x, ch in enumerate(row):
            c = PAL[ch]
            if c is not None:
                px[f * 16 + x, y] = c + (255,)

sheet.save("art.png")
