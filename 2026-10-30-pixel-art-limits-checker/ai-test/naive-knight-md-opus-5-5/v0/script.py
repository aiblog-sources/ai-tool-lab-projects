from PIL import Image
import numpy as np

# Mega Drive style palette: 9-bit colour (3 bits per channel), 16 entries, index 0 transparent
L = [0, 36, 73, 109, 146, 182, 219, 255]
def md(r, g, b):
    return (L[r], L[g], L[b], 255)

PAL = [
    (0, 0, 0, 0),     # 0 transparent
    md(0, 0, 1),      # 1 outline
    md(2, 2, 3),      # 2 steel dark
    md(4, 4, 5),      # 3 steel mid
    md(6, 6, 7),      # 4 steel light
    md(4, 0, 0),      # 5 plume dark
    md(6, 1, 1),      # 6 plume
    md(1, 1, 2),      # 7 visor slit
    md(3, 2, 1),      # 8 leather
    md(6, 5, 0),      # 9 gold
    md(0, 1, 4),      # 10 tunic dark
    md(1, 2, 6),      # 11 tunic
    md(5, 3, 2),      # 12 leather light
]

W = H = 32

def put(g, x, y, c):
    if 0 <= x < W and 0 <= y < H:
        g[y, x] = c

def rect(g, x0, y0, x1, y1, c):
    for y in range(y0, y1 + 1):
        for x in range(x0, x1 + 1):
            put(g, x, y, c)

def leg(g, hip_x, hip_y, foot_x, lift, base, hi):
    foot_y = 30 - lift
    shin_end = foot_y - 2
    n = max(1, shin_end - hip_y)
    for y in range(hip_y, shin_end + 1):
        t = (y - hip_y) / n
        x = round(hip_x + (foot_x - hip_x) * t)
        rect(g, x, y, x + 2, y, base)
        put(g, x + 2, y, hi)
    # boot
    rect(g, foot_x, foot_y - 1, foot_x + 2, foot_y - 1, 8)
    rect(g, foot_x, foot_y, foot_x + 3, foot_y, 8)
    put(g, foot_x + 1, foot_y - 1, 12)

def draw_frame(p, idx):
    g = np.zeros((H, W), dtype=np.uint8)
    bob, arm = p["bob"], p["arm"]

    # shield on back (behind everything)
    cx, cy = 11 - arm, 18 + bob
    for y in range(cy - 5, cy + 6):
        for x in range(cx - 4, cx + 5):
            d = ((x - cx) / 4.5) ** 2 + ((y - cy) / 5.5) ** 2
            if d <= 1.0:
                put(g, x, y, 9 if d > 0.6 else (10 if x < cx else 11))
    put(g, cx, cy, 9)

    # back leg (darker), then front leg
    hip_y = 22 + bob
    leg(g, 13, hip_y, 13 + p["b_foot"], p["b_lift"], 2, 2)
    leg(g, 16, hip_y, 16 + p["f_foot"], p["f_lift"], 3, 4)

    # torso
    rect(g, 12, 14 + bob, 19, 19 + bob, 3)
    rect(g, 12, 14 + bob, 12, 19 + bob, 2)
    rect(g, 17, 15 + bob, 18, 17 + bob, 4)
    rect(g, 12, 20 + bob, 19, 20 + bob, 8)
    put(g, 16, 20 + bob, 9)
    rect(g, 12, 21 + bob, 19, 22 + bob, 11)
    rect(g, 12, 21 + bob, 13, 22 + bob, 10)
    put(g, 16, 22 + bob, 10)

    # helmet
    hy = 6 + bob
    rect(g, 12, hy, 19, hy + 7, 3)
    put(g, 12, hy, 0); put(g, 19, hy, 0)
    rect(g, 12, hy + 1, 12, hy + 7, 2)
    rect(g, 14, hy + 1, 15, hy + 1, 4)
    put(g, 14, hy + 2, 4)
    rect(g, 15, hy + 3, 19, hy + 3, 7)          # visor slit
    rect(g, 17, hy + 5, 19, hy + 5, 2)          # breathing holes line
    put(g, 18, hy + 5, 7)
    put(g, 14, hy + 4, 9)                       # rivet
    rect(g, 13, hy + 7, 18, hy + 7, 2)

    # plume, tail flutters per frame
    f = idx % 2
    rect(g, 13, hy - 1, 16, hy - 1, 6)
    rect(g, 11, hy - 2, 15, hy - 2, 6)
    put(g, 15, hy - 2, 5)
    rect(g, 9, hy - 3 + f, 12, hy - 3 + f, 5)
    rect(g, 7, hy - 3 + f * 2, 8, hy - 3 + f * 2, 5)

    # front arm with sword
    rect(g, 15, 14 + bob, 18, 15 + bob, 4)      # pauldron
    put(g, 15, 15 + bob, 3)
    rect(g, 16 + arm, 16 + bob, 17 + arm, 18 + bob, 3)
    sy = 19 + bob
    rect(g, 16 + arm, sy, 18 + arm, sy + 1, 8)  # glove
    put(g, 17 + arm, sy, 12)
    put(g, 15 + arm, sy, 9)                     # pommel
    rect(g, 19 + arm, sy - 2, 19 + arm, sy + 2, 9)  # crossguard
    rect(g, 20 + arm, sy - 1, 27 + arm, sy - 1, 4)
    rect(g, 20 + arm, sy, 27 + arm, sy, 3)
    put(g, 28 + arm, sy, 4)                     # tip

    # outline pass
    out = g.copy()
    for y in range(H):
        for x in range(W):
            if g[y, x] == 0:
                for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                    nx, ny = x + dx, y + dy
                    if 0 <= nx < W and 0 <= ny < H and g[ny, nx] not in (0, 1):
                        out[y, x] = 1
                        break
    return out

frames = [
    dict(bob=0,  f_foot=4,  b_foot=-4, f_lift=0, b_lift=0, arm=-1),
    dict(bob=-1, f_foot=1,  b_foot=-1, f_lift=0, b_lift=2, arm=0),
    dict(bob=0,  f_foot=-4, b_foot=4,  f_lift=0, b_lift=0, arm=1),
    dict(bob=-1, f_foot=-1, b_foot=1,  f_lift=2, b_lift=0, arm=0),
]

sheet = np.zeros((H, W * 4), dtype=np.uint8)
for i, p in enumerate(frames):
    sheet[:, i * W:(i + 1) * W] = draw_frame(p, i)

rgba = np.array(PAL, dtype=np.uint8)[sheet]
Image.fromarray(rgba, "RGBA").save("art.png")
