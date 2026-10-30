# Knight walk cycle: 4 frames of 16x16 px, side by side in art.png (64x16 RGBA).
#
# Sega Master System constraints respected:
#  - Colors: every channel is one of 0, 85, 170, 255 (the SMS 6-bit CRAM grid).
#  - Palette: 8 opaque colors plus index 0 as transparent, within the 15-color
#    limit of one 16-color sprite palette.
#  - Tiles: each 16x16 frame is 4 native 8x8 tiles (16 tiles total for the sheet).
#    Index 0 is the transparent color, so the background is never drawn.

from PIL import Image

PALETTE = {
    '.': (0, 0, 0, 0),           # transparent (palette index 0)
    'K': (0, 0, 0, 255),         # outline black
    'S': (170, 170, 170, 255),   # steel
    'L': (255, 255, 255, 255),   # highlight
    'D': (85, 85, 85, 255),      # dark steel
    'B': (170, 85, 0, 255),      # leather / boots
    'R': (255, 0, 0, 255),       # red plume
    'Y': (255, 255, 0, 255),     # gold emblem
    'F': (255, 170, 85, 255),    # skin
}

# Sanity check: every channel must be a valid SMS color level.
SMS_LEVELS = {0, 85, 170, 255}
for key, rgba in PALETTE.items():
    assert all(c in SMS_LEVELS for c in rgba[:3]), f"Non-SMS color for {key}: {rgba}"
assert sum(1 for k in PALETTE if k != '.') <= 15, "Too many opaque colors for one sprite palette"

# Upper body shared by all frames (rows 0-10).
BODY = [
    "......RR",
    "....KKKKKK",
    "...KSSSSSSK",
    "...KSLSSFFK",
    "...KSSKKKKK",
    "...KDDDDDDK",
    "..KSSSSSSSSK",
    "..KSLSSSSSSK",
    "..KSSSYSSSSK",
    "..KSSSSSSSSKK",
    "...KBBBBBBK",
]

# Leg poses (rows 11-15), one set per frame of the cycle.
LEGS = [
    # Frame 1: contact, front leg forward
    [
        "...KSK...KSK",
        "...KSK...KSK",
        "...KSK..KSK.",
        "...KBK..KBK.",
        "..KBBBK.KBBBK",
    ],
    # Frame 2: passing, legs together
    [
        "....KSSK",
        "....KSSK",
        "....KSSK",
        "....KBBK",
        "...KBBBBK",
    ],
    # Frame 3: contact, opposite leg forward
    [
        "....KSK...KSK",
        "....KSK...KSK",
        ".....KSK.KSK",
        "....KBK..KBK",
        "...KBBBK.KBBBK",
    ],
    # Frame 4: passing, slight lean back
    [
        "....KSSK",
        "....KSSK",
        "...KSSK",
        "...KBBK",
        "..KBBBBK",
    ],
]


def build_frame(legs):
    rows = BODY + legs
    assert len(rows) == 16, "Each frame must be 16 rows tall"
    padded = [r.ljust(16, '.') for r in rows]
    for r in padded:
        assert len(r) == 16, "Each row must be 16 pixels wide"
    return padded


def main():
    sheet = Image.new("RGBA", (64, 16), (0, 0, 0, 0))
    for index, legs in enumerate(LEGS):
        frame = build_frame(legs)
        x0 = index * 16
        for y, row in enumerate(frame):
            for x, ch in enumerate(row):
                sheet.putpixel((x0 + x, y), PALETTE[ch])
    sheet.save("art.png")


if __name__ == "__main__":
    main()
