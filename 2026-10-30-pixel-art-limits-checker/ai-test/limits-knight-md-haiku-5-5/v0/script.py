import numpy as np
from PIL import Image

OUT = "art.png"
FW, FH, FRAMES = 32, 32, 4

# Mega Drive sprite palette line: 16 entries, index 0 = transparent.
# Colours use 9-bit RGB (3 bits per channel) -> each channel is a multiple of 36.
PAL = [
    (0, 0, 0),        # 0  transparent (not drawn)
    (36, 36, 36),     # 1  outline / visor
    (180, 180, 216), # 2  steel light
    (144, 144, 180), # 3  steel mid
    (108, 108, 144), # 4  steel dark
    (252, 216, 36),  # 5  gold
    (180, 144, 0),   # 6  gold dark
    (216, 36, 36),   # 7  red
    (144, 0, 0),     # 8  red dark
    (252, 180, 144), # 9  skin
    (144, 72, 36),   # 10 leather
    (72, 36, 0),     # 11 leather dark
    (36, 72, 216),   # 12 blue
    (0, 36, 144),    # 13 blue dark
    (252, 252, 252), # 14 white (spare)
]


def check_md_colors():
    for r, g, b in PAL:
        for v in (r, g, b):
            assert 0 <= v <= 252 and v % 36 == 0, f"non-MD colour channel {v}"
    assert len(PAL) <= 16, "a sprite palette line holds 16 entries"


def rect(a, x0, y0, x1, y1, c):
    """Fill inclusive rectangle with palette index c."""
    a[y0:y1 + 1, x0:x1 + 1] = c


def px(a, x, y, c):
    if 0 <= x < FW and 0 <= y < FH:
        a[y, x] = c


def draw_frame(t):
    a = np.zeros((FH, FW), np.uint8)
    # Body bobs up 1 px on passing frames.
    bob = 1 if t in (1, 3) else 0

    def U(x0, y0, x1, y1, c):  # upper body (affected by bob)
        rect(a, x0, y0 - bob, x1, y1 - bob, c)

    # Cape trailing behind (left)
    U(5, 14, 10, 24, 7)
    U(5, 21, 6, 24, 8)

    # Plume
    U(14, 1, 17, 3, 7)
    U(14, 4, 16, 4, 8)

    # Helmet
    U(10, 5, 20, 12, 1)
    U(11, 6, 19, 11, 3)
    U(12, 6, 13, 8, 2)
    U(13, 9, 18, 9, 1)      # visor slit
    U(20, 10, 21, 13, 4)    # nose guard

    # Gorget
    U(12, 12, 18, 13, 5)

    # Torso
    U(10, 14, 20, 21, 1)
    U(11, 14, 19, 20, 3)
    U(11, 14, 12, 19, 2)    # highlight
    U(11, 20, 19, 20, 6)    # belt

    # Hips
    U(11, 21, 19, 22, 11)

    # Shield (blue with gold cross), held in front
    U(18, 14, 25, 22, 13)
    U(19, 15, 24, 21, 12)
    U(21, 15, 22, 21, 5)
    U(19, 18, 24, 18, 5)

    # Sword raised forward, drawn over the shield
    for x, y in [(25, 17), (27, 19)]:       # crossguard
        px(a, x, y - bob, 5)
    px(a, 26, 18 - bob, 5)
    px(a, 25, 19 - bob, 6)                  # grip
    px(a, 24, 20 - bob, 6)                  # pommel
    for x, y in [(27, 17), (28, 16), (29, 15), (30, 14), (31, 13)]:
        px(a, x, y - bob, 2)                # blade

    # Legs: (front_dx, front_lift, back_dx, back_lift) per frame.
    # Legs are not bobbed; they stay on the ground line y=29.
    stride = {
        0: (2, 0, -2, 0),
        1: (0, 0, 0, 2),
        2: (-2, 0, 2, 0),
        3: (0, 2, 0, 0),
    }
    fdx, flift, bdx, blift = stride[t]

    def leg(cx, lift, c):
        rect(a, cx - 1, 22, cx + 1, 26 - lift, c)       # shin
        rect(a, cx - 2, 27 - lift, cx + 2, 29 - lift, 11)  # boot
        px(a, cx + 3, 28 - lift, 11)                    # toe

    leg(12 + bdx, blift, 4)   # back leg (darker)
    leg(17 + fdx, flift, 3)   # front leg
    return a


def main():
    check_md_colors()

    sheet_idx = np.zeros((FH, FW * FRAMES), np.uint8)
    for t in range(FRAMES):
        sheet_idx[:, t * FW:(t + 1) * FW] = draw_frame(t)

    # Each frame must use at most 15 opaque colours (index 1-15 of one palette line).
    for t in range(FRAMES):
        frame = sheet_idx[:, t * FW:(t + 1) * FW]
        used = set(np.unique(frame).tolist()) - {0}
        assert len(used) <= 15, f"frame {t} uses {len(used)} colours"

    rgba = np.zeros((FH, FW * FRAMES, 4), np.uint8)
    for idx in range(1, len(PAL)):
        mask = sheet_idx == idx
        rgba[mask, :3] = PAL[idx]
        rgba[mask, 3] = 255
    # index 0 stays (0,0,0,0): transparent

    Image.fromarray(rgba, "RGBA").save(OUT)


if __name__ == "__main__":
    main()
