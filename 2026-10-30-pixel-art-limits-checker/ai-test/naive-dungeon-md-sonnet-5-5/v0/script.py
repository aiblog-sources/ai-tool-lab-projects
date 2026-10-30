import numpy as np
from PIL import Image

W, H = 320, 224
rng = np.random.default_rng(7)
# image stored as Genesis colour levels (0-7 per channel), converted at the end
img = np.zeros((H, W, 3), dtype=np.int32)

def rect(x0, y0, x1, y1, c):
    img[y0:y1, x0:x1] = c

def px(x, y, c):
    if 0 <= x < W and 0 <= y < H:
        img[y, x] = c

# palette (level triples)
F1, F2, FM, FH = (4, 4, 4), (3, 3, 4), (2, 2, 2), (5, 5, 5)
W1, W2, WM, WH = (2, 2, 3), (2, 2, 2), (1, 1, 1), (3, 3, 4)
D1, D2, D3, DH = (4, 2, 1), (3, 1, 0), (2, 1, 0), (5, 3, 1)
IR, IRH = (2, 2, 2), (4, 4, 4)

# ---------- floor ----------
for ty in range(3, 13):
    for tx in range(1, 19):
        x0, y0 = tx * 16, ty * 16
        base = F1 if rng.random() < 0.6 else F2
        rect(x0, y0, x0 + 16, y0 + 16, base)
        rect(x0, y0 + 15, x0 + 16, y0 + 16, FM)
        rect(x0 + 15, y0, x0 + 16, y0 + 16, FM)
        rect(x0, y0, x0 + 15, y0 + 1, FH)
        rect(x0, y0, x0 + 1, y0 + 15, FH)
        for _ in range(5):
            px(x0 + rng.integers(2, 14), y0 + rng.integers(2, 14),
               FM if rng.random() < 0.5 else FH)
        if rng.random() < 0.15:  # crack
            cx, cy = x0 + rng.integers(3, 10), y0 + rng.integers(3, 10)
            for i in range(5):
                px(cx + i, cy + i // 2 + (i % 2), FM)

# ---------- walls ----------
def brick_area(xa, ya, xb, yb):
    for y in range(ya, yb, 8):
        off = 8 if ((y - ya) // 8) % 2 else 0
        for x in range(xa - off, xb, 16):
            bx0, bx1 = max(x, xa), min(x + 16, xb)
            if bx0 >= bx1:
                continue
            by1 = min(y + 8, yb)
            base = W1 if rng.random() < 0.6 else W2
            rect(bx0, y, bx1, by1, base)
            rect(bx0, y, bx1, y + 1, WH)
            if x >= xa:
                rect(x, y, x + 1, by1, WM)
            rect(bx0, by1 - 1, bx1, by1, WM)
            px(bx0 + rng.integers(1, max(2, bx1 - bx0 - 1)), y + rng.integers(2, 6), WM)

brick_area(0, 0, 320, 48)      # top wall face
brick_area(0, 48, 16, 224)     # left wall
brick_area(304, 48, 320, 224)  # right wall
brick_area(16, 208, 304, 224)  # bottom wall
rect(0, 46, 320, 48, WM)
rect(16, 208, 304, 209, WM)

# wall shadows on floor
img[48:50, 16:304] = np.maximum(img[48:50, 16:304] - 2, 0)
img[50:52, 16:304] = np.maximum(img[50:52, 16:304] - 1, 0)
for y in range(52, 55):
    for x in range(16, 304):
        if (x + y) % 2 == 0:
            img[y, x] = np.maximum(img[y, x] - 1, 0)
for x in range(16, 20):
    img[49:208, x] = np.maximum(img[49:208, x] - (1 if x < 18 else 0), 0)
for x in range(300, 304):
    img[49:208, x] = np.maximum(img[49:208, x] - (1 if x >= 302 else 0), 0)

# ---------- door ----------
dx0, dx1, dy0 = 140, 180, 12
rect(dx0 - 4, dy0 - 4, dx1 + 4, 48, WM)        # stone frame shadow
rect(dx0 - 3, dy0 - 3, dx1 + 3, 48, WH)        # frame
rect(dx0 - 1, dy0 - 1, dx1 + 1, 48, D3)        # inner dark
for x in range(dx0, dx1):
    pl = (x - dx0) // 8
    col = D1 if pl % 2 == 0 else D2
    rect(x, dy0, x + 1, 46, col)
    if (x - dx0) % 8 == 0:
        rect(x, dy0, x + 1, 46, D3)
    if (x - dx0) % 8 == 1:
        rect(x, dy0, x + 1, 46, DH)
for y in (dy0 + 6, 38):                         # iron bands
    rect(dx0, y, dx1, y + 3, IR)
    rect(dx0, y, dx1, y + 1, IRH)
    for x in range(dx0 + 3, dx1, 8):
        px(x, y + 1, IRH)
rect(dx0 + 1, dy0, dx1 - 1, dy0 + 1, D3)
rect(dx0 + 29, 28, dx0 + 33, 32, IRH)            # handle ring
rect(dx0 + 30, 29, dx0 + 32, 31, D3)
rect(dx0 - 4, 46, dx1 + 4, 48, FM)
rect(dx0 - 2, 48, dx1 + 2, 50, F2)               # threshold

# ---------- torches ----------
torches = [(80, 26), (240, 26)]
for tx, ty in torches:
    rect(tx - 1, ty + 2, tx + 2, ty + 14, D2)    # handle
    rect(tx - 1, ty + 2, tx, ty + 14, D1)
    rect(tx - 3, ty + 1, tx + 4, ty + 3, IR)     # cup
    rect(tx - 3, ty + 1, tx + 4, ty + 2, IRH)
    rect(tx - 2, ty + 13, tx + 3, ty + 15, IR)   # bracket

# ---------- glow (applied before flames) ----------
yy, xx = np.mgrid[0:H, 0:W]
for tx, ty in torches:
    d = np.sqrt((xx - tx) ** 2 + ((yy - ty) * 1.1) ** 2)
    inner = d < 22
    mid = (d >= 22) & (d < 46) & (((xx + yy) % 2) == 0)
    outer = (d >= 46) & (d < 62) & (((xx % 2 == 0) & (yy % 2 == 0)))
    m = inner | mid | outer
    add = np.zeros((H, W, 1), dtype=np.int32)
    add[m] = 1
    inner3 = inner & (d < 12)
    add[inner3] = 2
    tint = np.array([1, 1, 0])
    img[:] = np.minimum(img + add * np.array([1, 1, 0]) + (add > 0) * np.array([0, 0, 0]), 7)
    # keep blue from lifting too much: tint warm
    img[:, :, 2] = np.minimum(img[:, :, 2], np.maximum(img[:, :, 0] - 1, 0) + 1)

# ---------- flames ----------
flame = [(1, 1), (2, 2), (3, 3), (4, 3), (5, 4), (6, 3), (7, 3), (8, 2), (9, 1), (10, 1)]
for tx, ty in torches:
    base = ty
    for i, (h, w) in enumerate(flame):
        y = base - h
        sway = 1 if i in (7, 8, 9) else 0
        rect(tx - w + sway, y, tx + w + 1 + sway, y + 1, (6, 1, 0))
    for i, (h, w) in enumerate(flame[:8]):
        w2 = max(w - 1, 0)
        y = base - h
        rect(tx - w2, y, tx + w2 + 1, y + 1, (7, 4, 0))
    for h in range(1, 5):
        w3 = 1 if h < 4 else 0
        rect(tx - w3, base - h, tx + w3 + 1, base - h + 1, (7, 7, 2))
    px(tx + 2, base - 12, (7, 3, 0))

# ---------- output ----------
lv = np.array([0, 36, 72, 108, 144, 180, 216, 252], dtype=np.uint8)
out = lv[np.clip(img, 0, 7)]
Image.fromarray(out, "RGB").save("art.png")
