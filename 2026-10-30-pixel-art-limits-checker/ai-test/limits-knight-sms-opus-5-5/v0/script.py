from PIL import Image

# Sega Master System sprite constraints respected:
# - Every color uses only the SMS 6-bit RGB levels (0, 85, 170, 255 per channel).
# - The sheet uses one 16-color sprite palette, with index 0 transparent.
#   That leaves 15 visible colors, and this art uses 10 of them.
# - Each 16x16 frame is 4 tiles of 8x8 (or 2 sprites in 8x16 mode).
#   A frame needs only 2 hardware sprites per scanline, well under the limit of 8.
# - The output is an indexed PNG whose palette order maps directly to SMS CRAM.

PAL = {
    '.': (0, 0, 0),         # 0  transparent
    'K': (0, 0, 0),         # 1  black outline
    'D': (85, 85, 85),      # 2  dark steel
    'L': (170, 170, 170),   # 3  steel
    'W': (255, 255, 255),   # 4  highlight
    'r': (170, 0, 0),       # 5  dark plume
    'R': (255, 0, 0),       # 6  plume
    'B': (0, 0, 170),       # 7  tunic
    'b': (0, 85, 255),      # 8  tunic light
    'Y': (255, 170, 0),     # 9  gold belt
    'N': (170, 85, 0),      # 10 leather boots
}
ORDER = list(PAL.keys())
for c in PAL.values():
    assert all(v in (0, 85, 170, 255) for v in c)
assert len(ORDER) <= 16

UPPER = [
    "....rRR.........",
    "...rRRKKKK......",
    "...r.KLLWLK.....",
    ".....KLLLWLK....",
    "....KDLLLLLK....",
    "....KDLKKKKK....",
    "....KDLLLLLK....",
    ".....KDDDDK.....",
    "...KKBBBbbKK....",
    "..KDLKBBBbKLK...",
    "..KDKKYYYYKLLK..",
    "...K.KBBBBKK....",
]

LEGS = [
    [  # contact, front leg forward
        "....KDK.KLK.....",
        "...KDK...KLK....",
        "..KNNK...KNNK...",
        "..KKK.....KKKK..",
    ],
    [  # passing
        ".....KDLK.......",
        ".....KDLK.......",
        ".....KNNNK......",
        ".....KKKKK......",
    ],
    [  # contact, other leg forward
        "....KLK.KDK.....",
        "...KLK...KDK....",
        "..KNNK...KNNK...",
        "..KKK.....KKKK..",
    ],
    [  # passing, other leg
        ".....KLDK.......",
        ".....KLDK.......",
        ".....KNNNK......",
        ".....KKKKK......",
    ],
]

# Arm swing: alternate rows 9-10 slightly per frame
ARMS_ALT = [
    "..KDLKBBBbKLK...",
    "..KDKKYYYYKLLK..",
]
ARMS_SWING = [
    "...KDKBBBbKLK...",
    "...KLKYYYYKLLK..",
]

img = Image.new("P", (64, 16), 0)
flat = []
for k in ORDER:
    flat += list(PAL[k])
flat += [0, 0, 0] * (256 - len(ORDER))
img.putpalette(flat)

for f in range(4):
    rows = list(UPPER)
    if f in (1, 3):
        rows[9], rows[10] = ARMS_SWING
    else:
        rows[9], rows[10] = ARMS_ALT
    rows += LEGS[f]
    assert len(rows) == 16
    for y, row in enumerate(rows):
        assert len(row) <= 16
        row = row.ljust(16, '.')
        for x, ch in enumerate(row):
            img.putpixel((f * 16 + x, y), ORDER.index(ch))

img.save("art.png", transparency=0)
