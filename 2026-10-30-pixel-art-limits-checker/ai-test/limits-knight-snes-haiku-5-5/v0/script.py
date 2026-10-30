# Knight walk cycle sprite sheet for SNES (128x32, four 32x32 frames).
#
# SNES constraints respected:
# - 4bpp sprites: each OBJ palette has 16 entries, index 0 is always transparent,
#   so at most 15 opaque colours are used (indices 1..15).
# - Colours are quantised to the SNES 15-bit BGR format (5 bits per channel).
# - Each 32x32 frame is one large OBJ built from 16 8x8 tiles, so the sheet
#   needs 64 tiles in total, well within the sprite VRAM limit.
# - Feet stay on one ground line and the bob is only 1 px, to keep the motion readable.

from PIL import Image
import numpy as np

OUT = "art.png"
FRAME = 32
NFRAMES = 4

# Palette index 0 = transparent. Indices 1..12 are the opaque colours used.
PAL = {
    1: (16, 24, 32),       # outline
    2: (200, 208, 216),    # armour light
    3: (128, 144, 160),    # armour mid
    4: (80, 90, 104),      # armour shadow
    5: (192, 32, 32),      # plume
    6: (240, 184, 144),    # skin
    7: (128, 72, 32),      # leather
    8: (64, 48, 32),       # boots
    9: (224, 176, 64),     # gold
    10: (32, 80, 192),     # shield
    11: (96, 160, 240),    # shield highlight
    12: (232, 232, 240),   # blade
}


def to_snes(rgb):
    """Quantise an 8-bit RGB colour to the SNES 5-bit-per-channel grid."""
    out = []
    for v in rgb:
        v5 = round(v * 31 / 255)
        out.append((v5 << 3) | (v5 >> 2))
    return out


def fill(g, x0, y0, x1, y1, c):
    g[y0:y1 + 1, x0:x1 + 1] = c


def add_outline(g):
    """Add a 1px outline (index 1) around the silhouette."""
    opaque = g != 0
    pad = np.pad(opaque, 1)
    near = (pad[:-2, 1:-1] | pad[2:, 1:-1] |
            pad[1:-1, :-2] | pad[1:-1, 2:])
    out = g.copy()
    out[(~opaque) & near] = 1
    return out


def draw_frame(phase):
    """Draw one walk-cycle frame. phase 0..3: contact, passing, contact, passing."""
    g = np.zeros((FRAME, FRAME), np.uint8)
    t = [0, -1, 0, -1][phase]     # torso/head bob
    stride = [3, 0, -3, 0][phase]  # leg spread
    arm = [0, 1, 0, -1][phase]     # sword arm sway

    # Legs: back leg first, then front leg overlaps it.
    for x, shade in ((14 - stride, 4), (18 + stride, 3)):
        fill(g, x - 1, 22, x + 1, 26, shade)
        fill(g, x - 1, 27, x + 1, 29, 8)

    # Plume (trails behind, to the left)
    fill(g, 10, 1 + t, 15, 3 + t, 5)

    # Helmet
    fill(g, 12, 4 + t, 20, 11 + t, 2)
    fill(g, 12, 4 + t, 13, 11 + t, 3)
    fill(g, 15, 8 + t, 20, 8 + t, 4)     # visor slit
    fill(g, 17, 9 + t, 19, 10 + t, 6)    # face below visor
    fill(g, 17, 11 + t, 20, 11 + t, 4)   # chin guard

    # Torso
    fill(g, 11, 12 + t, 20, 22, 3)
    fill(g, 17, 13 + t, 19, 20, 2)       # chest highlight
    fill(g, 11, 19 + t, 20, 19 + t, 7)   # belt
    fill(g, 15, 19 + t, 16, 19 + t, 9)   # buckle

    # Shield on the back side (left)
    fill(g, 7, 13 + t, 11, 20 + t, 10)
    fill(g, 8, 14 + t, 10, 19 + t, 11)
    fill(g, 9, 16 + t, 9, 17 + t, 9)

    # Sword arm, sword and hand
    fill(g, 21, 13 + t, 22, 16 + t, 3)
    fill(g, 23, 15 + t + arm, 23, 18 + t + arm, 9)    # hilt
    fill(g, 24, 16 + t + arm, 30, 17 + t + arm, 12)   # blade
    fill(g, 21, 17 + t + arm, 22, 18 + t + arm, 6)    # hand

    return add_outline(g)


# Assemble the four frames side by side.
sheet = np.zeros((FRAME, FRAME * NFRAMES), np.uint8)
for f in range(NFRAMES):
    sheet[:, f * FRAME:(f + 1) * FRAME] = draw_frame(f)

# Check the SNES 4bpp palette limit: at most 15 opaque colours.
used = set(int(v) for v in np.unique(sheet)) - {0}
assert len(used) <= 15, "too many colours for one 4bpp sprite palette"
assert used <= set(PAL.keys()), "unknown palette index used"
assert sheet.shape == (FRAME, FRAME * NFRAMES)

# Convert indices to RGBA. Index 0 becomes fully transparent.
rgba = np.zeros((FRAME, FRAME * NFRAMES, 4), np.uint8)
for idx, rgb in PAL.items():
    mask = sheet == idx
    rgba[mask, :3] = to_snes(rgb)
    rgba[mask, 3] = 255

Image.fromarray(rgba, "RGBA").save(OUT)
