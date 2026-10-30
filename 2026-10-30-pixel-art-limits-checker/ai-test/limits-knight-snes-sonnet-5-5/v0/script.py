from PIL import Image
import numpy as np

# 16-color SNES palette (index 0 = transparent), channels are 5-bit values expanded to 8-bit
def c15(r, g, b):
    e = lambda v: (v << 3) | (v >> 2)
    return (e(r), e(g), e(b))

PAL = [
    c15(0, 0, 0),      # 0 transparent
    c15(3, 3, 8),      # 1 outline
    c15(10, 11, 16),   # 2 steel dark
    c15(16, 18, 22),   # 3 steel mid
    c15(22, 25, 28),   # 4 steel light
    c15(30, 31, 31),   # 5 highlight
    c15(26, 5, 6),     # 6 red
    c15(16, 2, 5),     # 7 dark red
    c15(6, 10, 24),    # 8 blue
    c15(3, 5, 15),     # 9 dark blue
    c15(29, 22, 17),   # 10 skin
    c15(18, 11, 5),    # 11 brown
    c15(10, 6, 3),     # 12 dark brown
    c15(30, 24, 6),    # 13 gold
    c15(24, 17, 3),    # 14 dark gold
    c15(12, 12, 14),   # 15 spare grey
]

def draw_frame(f):
    a = np.zeros((32, 32), dtype=np.uint8)

    def rect(x0, y0, x1, y1, c):
        a[y0:y1 + 1, x0:x1 + 1] = c

    def px(x, y, c):
        if 0 <= x < 32 and 0 <= y < 32:
            a[y, x] = c

    bob = [0, -1, 0, -1][f]
    legs = [((3, 0), (-3, 1)), ((0, 0), (1, 2)),
            ((-3, 1), (3, 0)), ((1, 2), (0, 0))][f]
    (adx, alift), (bdx, blift) = legs

    def leg(hx, dx, lift, body, boot, toe):
        hip = 23 + bob
        foot = 28 - lift
        span = max(1, foot - hip)
        for y in range(hip, foot + 1):
            o = round(dx * (y - hip) / span)
            c = boot if y >= foot - 1 else body
            rect(hx + o, y, hx + o + 2, y, c)
        o = dx
        px(hx + o + 3, foot, toe)

    # far leg
    leg(14, bdx, blift, 2, 12, 12)
    # sword blade (behind shield arm, in front hand)
    rect(24, 7 + bob, 24, 17 + bob, 4)
    rect(25, 8 + bob, 25, 17 + bob, 3)
    px(24, 6 + bob, 5)
    rect(22, 18 + bob, 26, 18 + bob, 13)
    rect(24, 19 + bob, 25, 20 + bob, 11)
    # torso
    rect(11, 13 + bob, 19, 20 + bob, 3)
    rect(11, 13 + bob, 12, 20 + bob, 2)
    rect(17, 14 + bob, 18, 15 + bob, 4)
    # belt and tunic
    rect(11, 21 + bob, 19, 21 + bob, 11)
    px(15, 21 + bob, 13)
    rect(11, 22 + bob, 19, 24 + bob, 8)
    rect(11, 22 + bob, 12, 24 + bob, 9)
    # head / helmet
    rect(11, 5 + bob, 19, 12 + bob, 3)
    rect(11, 5 + bob, 12, 12 + bob, 2)
    rect(13, 5 + bob, 17, 6 + bob, 4)
    px(14, 5 + bob, 5)
    rect(15, 9 + bob, 19, 9 + bob, 1)
    rect(16, 10 + bob, 19, 10 + bob, 10)
    rect(13, 12 + bob, 19, 12 + bob, 2)
    # plume
    pl = [0, 1, 0, 1][f]
    rect(9, 3 + bob, 13, 4 + bob, 6)
    rect(7, 4 + bob + pl, 10, 6 + bob + pl, 6)
    rect(7, 6 + bob + pl, 9, 7 + bob + pl, 7)
    rect(12, 4 + bob, 14, 4 + bob, 7)
    # arm + shield
    rect(17, 16 + bob, 21, 17 + bob, 3)
    px(21, 17 + bob, 10)
    rect(12, 15 + bob, 18, 22 + bob, 14)
    rect(13, 16 + bob, 17, 21 + bob, 6)
    rect(13, 19 + bob, 17, 21 + bob, 7)
    rect(14, 17 + bob, 16, 18 + bob, 13)
    px(15, 17 + bob, 5)
    # near leg
    leg(14, adx, alift, 3, 11, 11)
    # re-darken boot tips
    # outline
    out = a.copy()
    h, w = a.shape
    for y in range(h):
        for x in range(w):
            if a[y, x] == 0:
                for dy, dx in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                    yy, xx = y + dy, x + dx
                    if 0 <= yy < h and 0 <= xx < w and a[yy, xx] != 0:
                        out[y, x] = 1
                        break
    return out

sheet = np.zeros((32, 128), dtype=np.uint8)
for i in range(4):
    sheet[:, i * 32:(i + 1) * 32] = draw_frame(i)

img = Image.fromarray(sheet, mode="P")
flat = []
for c in PAL:
    flat.extend(c)
flat.extend([0] * (768 - len(flat)))
img.putpalette(flat)
img.info["transparency"] = 0
img.save("art.png", transparency=0)
