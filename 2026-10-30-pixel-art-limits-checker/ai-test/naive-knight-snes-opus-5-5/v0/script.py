#!/usr/bin/env python3
"""4-frame walk cycle of a small knight facing right (SNES style).
Output: art.png, 128x32 with 4 frames of 32x32 and a transparent background."""
from PIL import Image
import numpy as np

W = H = 32
FRAMES = 4

# SNES-friendly palette: 5-bit channels (multiples of 8), 15 colors + transparent
PAL = {
    0: (0, 0, 0, 0),           # transparent
    1: (24, 16, 32, 255),      # outline
    2: (80, 88, 112, 255),     # steel dark
    3: (136, 144, 168, 255),   # steel mid
    4: (192, 200, 216, 255),   # steel light
    5: (248, 248, 248, 255),   # highlight
    6: (112, 24, 40, 255),     # cape dark
    7: (176, 40, 56, 255),     # cape mid
    8: (224, 72, 72, 255),     # cape light
    9: (192, 136, 32, 255),    # gold
    10: (248, 208, 88, 255),   # gold light
    11: (88, 56, 40, 255),     # leather dark
    12: (144, 96, 56, 255),    # leather
    13: (40, 32, 56, 255),     # visor shadow
    14: (56, 56, 80, 255),     # far-side steel (darkest)
}

def grid():
    return np.zeros((H, W), dtype=np.uint8)

def put(g, x, y, c):
    if 0 <= x < W and 0 <= y < H:
        g[y, x] = c

def span(g, y, x0, x1, c):
    for x in range(x0, x1 + 1):
        put(g, x, y, c)

def outline(g):
    """Return layer with a 1px outline around its filled pixels."""
    out = g.copy()
    filled = g != 0
    for y in range(H):
        for x in range(W):
            if filled[y, x]:
                continue
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                nx, ny = x + dx, y + dy
                if 0 <= nx < W and 0 <= ny < H and filled[ny, nx]:
                    out[y, x] = 1
                    break
    return out

def composite(base, layer):
    m = layer != 0
    base[m] = layer[m]

# ---------------------------------------------------------------- parts
def draw_leg(g, hipx, hipy, dx, lift, cols, boot):
    shade, main, light = cols
    footy = 29 - lift
    top_boot = footy - 1
    last = top_boot - 1
    rows = max(1, last - hipy)
    for y in range(hipy, last + 1):
        t = (y - hipy) / rows
        cx = int(round(hipx + dx * t))
        put(g, cx - 1, y, shade)
        put(g, cx, y, main)
        put(g, cx + 1, y, light)
    # knee pad when the leg is lifted / mid stride
    ky = hipy + (last - hipy) // 2
    kx = int(round(hipx + dx * 0.5))
    put(g, kx + 1, ky, light)
    if lift:
        put(g, kx + 2, ky, main)
    # boot (toe points right)
    bx = hipx + dx
    bd, bm = boot
    span(g, top_boot, bx - 1, bx + 1, bm)
    span(g, footy, bx - 1, bx + 2, bd)
    put(g, bx + 1, top_boot, bm)
    put(g, bx, top_boot, bm if bm != bd else bd)

def draw_arm(g, sx, sy, adx, cols, glove):
    shade, main, light = cols
    for i in range(5):
        t = i / 4
        cx = int(round(sx + adx * t))
        put(g, cx, sy + i, shade)
        put(g, cx + 1, sy + i, main if i % 4 else light)
    hx = sx + adx
    gd, gm = glove
    span(g, sy + 5, hx, hx + 1, gm)
    span(g, sy + 6, hx, hx + 1, gd)

def draw_frame(f):
    bob = -1 if f in (1, 3) else 0
    near_dx = [3, 0, -3, 0][f]
    far_dx = [-3, 0, 3, 0][f]
    near_lift = [0, 0, 0, 2][f]
    far_lift = [0, 2, 0, 0][f]
    near_arm = [-2, 0, 2, 0][f]
    far_arm = [2, 0, -2, 0][f]
    sway = [0, 1, 0, 1][f]

    img = grid()

    # --- back group: cape, far leg, far arm
    back = grid()
    for y in range(14 + bob, 26 + bob):
        k = y - (14 + bob)
        left = 12 - int(k * 0.45) - (sway if k > 6 else 0)
        right = 14
        span(back, y, left, right, 7)
        put(back, left, y, 6)
        if k > 2 and left + 1 < right:
            put(back, left + 1, y, 6 if k % 3 == 0 else 7)
        if k < 4:
            put(back, right - 1, y, 8)
    # cape hem
    hem_y = 25 + bob
    for x in range(12 - int(11 * 0.45) - sway, 15):
        if (x + f) % 3 == 0:
            put(back, x, hem_y + 1, 6)
    draw_leg(back, 15, 23 + bob, far_dx, far_lift, (14, 2, 2), (13, 14))
    draw_arm(back, 14, 16 + bob, far_arm, (14, 2, 2), (13, 11))
    composite(img, outline(back))

    # --- body group: plume, torso, belt, helmet
    body = grid()
    tip = [0, 1, 0, -1][f]
    span(body, 2 + bob, 12, 15, 7)
    span(body, 3 + bob, 10, 16, 7)
    span(body, 4 + bob, 8 + tip, 13, 7)
    span(body, 5 + bob, 7 + tip, 11, 6)
    span(body, 6 + bob, 7 + tip, 9, 6)
    span(body, 2 + bob, 13, 14, 8)
    span(body, 3 + bob, 12, 14, 8)
    put(body, 9 + tip, 4 + bob, 6)

    # gorget
    span(body, 14 + bob, 14, 18, 2)
    # torso / breastplate
    torso = {15: (13, 19), 16: (12, 20), 17: (12, 20), 18: (12, 20),
             19: (12, 20), 20: (13, 19)}
    for y, (x0, x1) in torso.items():
        for x in range(x0, x1 + 1):
            c = 3
            if x <= 13:
                c = 2
            elif x >= 18:
                c = 4
            put(body, x, y + bob, c)
    put(body, 18, 16 + bob, 5)
    put(body, 19, 17 + bob, 5)
    # belt + buckle
    span(body, 21 + bob, 12, 20, 12)
    put(body, 12, 21 + bob, 11)
    put(body, 17, 21 + bob, 10)
    put(body, 18, 21 + bob, 9)
    # tassets
    span(body, 22 + bob, 13, 19, 2)
    put(body, 18, 22 + bob, 3)

    # helmet
    helm = {4: (14, 18), 5: (13, 19), 6: (12, 20), 7: (12, 21), 8: (12, 21),
            9: (12, 21), 10: (12, 21), 11: (12, 21), 12: (12, 20), 13: (13, 19)}
    for y, (x0, x1) in helm.items():
        for x in range(x0, x1 + 1):
            c = 3
            if y <= 7 and x >= 15:
                c = 4
            if x <= 13 or y >= 12:
                c = 2
            put(body, x, y + bob, c)
    put(body, 17, 5 + bob, 5)
    put(body, 18, 6 + bob, 5)
    put(body, 19, 7 + bob, 4)
    # crest ridge
    for y in range(4, 9):
        put(body, 15, y + bob, 2 if y > 4 else 3)
    # visor slit and breathing holes
    span(body, 9 + bob, 16, 21, 13)
    put(body, 18, 11 + bob, 13)
    put(body, 20, 11 + bob, 13)
    put(body, 21, 8 + bob, 4)
    # gold band on helmet
    span(body, 13 + bob, 14, 18, 9)
    put(body, 17, 13 + bob, 10)
    composite(img, outline(body))

    # --- near leg
    leg = grid()
    draw_leg(leg, 17, 23 + bob, near_dx, near_lift, (2, 3, 4), (2, 3))
    composite(img, outline(leg))

    # --- scabbard + hilt on the near hip
    sc = grid()
    for i in range(6):
        put(sc, 13 - i, 22 + bob + (i * 2) // 3, 11 if i % 2 else 12)
    put(sc, 7, 26 + bob, 9)
    put(sc, 14, 21 + bob, 9)
    put(sc, 15, 20 + bob, 10)
    put(sc, 14, 20 + bob, 9)
    composite(img, outline(sc))

    # --- near arm with pauldron
    arm = grid()
    draw_arm(arm, 16, 17 + bob, near_arm, (2, 3, 4), (11, 12))
    paul = {14: (15, 18), 15: (14, 19), 16: (14, 19), 17: (15, 18)}
    for y, (x0, x1) in paul.items():
        for x in range(x0, x1 + 1):
            c = 3
            if y <= 15 and x >= 16:
                c = 4
            if y == 17 or x == 14:
                c = 2
            put(arm, x, y + bob, c)
    put(arm, 17, 14 + bob, 5)
    put(arm, 15, 17 + bob, 9)
    put(arm, 18, 17 + bob, 9)
    composite(img, outline(arm))

    return img

# ---------------------------------------------------------------- build sheet
sheet = np.zeros((H, W * FRAMES, 4), dtype=np.uint8)
for f in range(FRAMES):
    g = draw_frame(f)
    for y in range(H):
        for x in range(W):
            sheet[y, f * W + x] = PAL[int(g[y, x])]

Image.fromarray(sheet, "RGBA").save("art.png")
