from PIL import Image

# NES sprite rules this art follows:
# - Index 0 of a sprite palette is always transparent.
# - Each sprite palette has 3 opaque colors (indices 1-3).
# - Each 16x16 frame is four 8x8 tiles (or two 8x16 sprites). Every tile uses
#   one palette, so the whole frame uses one palette here, with 3 opaque colors.
# Colors are approximate RGB values for the NES PPU entries noted in the comments.
PALETTE = {
    "K": (0, 0, 0, 255),        # NES $0F black: outline, visor
    "A": (188, 188, 188, 255),  # NES $10 light gray: armor
    "R": (184, 30, 0, 255),     # NES $16 red: plume, chest emblem
}

# Upper body (rows 0-10), shared by all frames.
UPPER = [
    "......RR........",
    ".....KKKKK......",
    "....KAAAAK......",
    "....KAKKKK......",
    "....KAAAAK......",
    "....KKKKKK......",
    "...KAAAAAAK.....",
    "..KAAAAAAAAK....",
    "..KAAARAAAAK....",
    "..KAAAAAAAAK....",
    "..KKKKKKKKKK....",
]

# Legs (rows 11-15) for each pose.
LEGS_CONTACT_RIGHT_FWD = [   # right leg forward
    "...KAK...KAK....",
    "...KAK...KAK....",
    "...KKK...KKKK...",
    "...KAK...KAAK...",
    "..KKKK....KKKK..",
]
LEGS_CONTACT_LEFT_FWD = [    # left leg forward
    "...KAK..KAK.....",
    "...KAK..KAK.....",
    "..KKKKK.KKKK....",
    "..KAAAK.KAAK....",
    "..KKKKK..KKKK...",
]
LEGS_PASSING = [             # legs together
    "....KAKAK.......",
    "....KAKAK.......",
    "....KKKKK.......",
    "....KAAAK.......",
    "...KKKKKK.......",
]


def pad(row):
    return row.ljust(16, ".")[:16]


def bob(frame):
    """Drop the body by 1 px for a bounce, keeping the legs in place."""
    return ["." * 16] + frame[:10] + frame[11:]


frame_right_fwd = [pad(r) for r in UPPER + LEGS_CONTACT_RIGHT_FWD]
frame_passing = [pad(r) for r in UPPER + LEGS_PASSING]
frame_left_fwd = [pad(r) for r in UPPER + LEGS_CONTACT_LEFT_FWD]
frame_bounce = bob(frame_passing)

FRAMES = [frame_right_fwd, frame_passing, frame_left_fwd, frame_bounce]

# Validate against NES constraints: 16x16 frame, at most 3 opaque colors
# per 8x8 tile, and 3 opaque colors per frame palette.
for f in FRAMES:
    assert len(f) == 16 and all(len(r) == 16 for r in f)
    frame_colors = {c for r in f for c in r if c != "."}
    assert len(frame_colors) <= 3
    for ty in range(0, 16, 8):
        for tx in range(0, 16, 8):
            tile_colors = {f[y][x] for y in range(ty, ty + 8)
                           for x in range(tx, tx + 8) if f[y][x] != "."}
            assert len(tile_colors) <= 3

# Compose the 64x16 sheet with a transparent background.
sheet = Image.new("RGBA", (64, 16), (0, 0, 0, 0))
for i, frame in enumerate(FRAMES):
    for y, row in enumerate(frame):
        for x, ch in enumerate(row):
            if ch != ".":
                sheet.putpixel((i * 16 + x, y), PALETTE[ch])

sheet.save("art.png")
