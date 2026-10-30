from PIL import Image

# NES-style sprite palette: transparent + 3 colors
PAL = {
    ".": (0, 0, 0, 0),
    "K": (0, 0, 0, 255),        # outline (NES $0F)
    "S": (188, 188, 188, 255),  # steel   (NES $10)
    "R": (168, 16, 0, 255),     # red     (NES $06)
}

# Upper body: rows 1..11 (row 0 left empty so passing frames can bob up 1px)
UPPER = [
    "................",
    ".....RR.........",
    "....RRKKKK......",
    "....RKSSSSK.....",
    ".....KSSSSSK....",
    ".....KSSKKKK....",
    ".....KSSSSSK....",
    ".....KKSSSKK....",
    "....KSSSSSSK....",
    "...KSRRRRSSK....",
    "...KSRRRRKSK....",
    "....KKKKKKK.....",
]

# Contact poses: legs on rows 12..15
LEGS_A = [
    "....KSSK.KSK....",
    "...KSSK...KSK...",
    "...KSK....KSSK..",
    "...KKK....KKKKK.",
]
LEGS_C = [
    ".....KSK.KSK....",
    "....KSK...KSK...",
    "...KSSK...KSK...",
    "...KKK...KKKK...",
]
# Passing poses: legs on rows 11..15 (body raised 1px)
LEGS_B = [
    ".....KSSSSK.....",
    ".....KSSSSK.....",
    ".....KSKSSK.....",
    ".....KSKSSK.....",
    ".....KKKKKKK....",
]
LEGS_D = [
    ".....KSSSSK.....",
    ".....KSSSSK.....",
    ".....KSSKSK.....",
    "....KSK.KSK.....",
    "....KK..KKKK....",
]


def build(legs, passing):
    grid = [["."] * 16 for _ in range(16)]
    off = -1 if passing else 0
    for y, row in enumerate(UPPER):
        ty = y + off
        if 0 <= ty < 16:
            for x, c in enumerate(row.ljust(16, ".")[:16]):
                if c != ".":
                    grid[ty][x] = c
    start = 11 if passing else 12
    for i, row in enumerate(legs):
        for x, c in enumerate(row.ljust(16, ".")[:16]):
            if c != ".":
                grid[start + i][x] = c
    return grid


frames = [
    build(LEGS_A, False),
    build(LEGS_B, True),
    build(LEGS_C, False),
    build(LEGS_D, True),
]

img = Image.new("RGBA", (64, 16), (0, 0, 0, 0))
px = img.load()
for f, grid in enumerate(frames):
    for y in range(16):
        for x in range(16):
            px[f * 16 + x, y] = PAL[grid[y][x]]

img.save("art.png")
