from PIL import Image

# Sega Master System: 6-bit palette (each channel 0, 85, 170 or 255).
# Sprite palette: 16 entries, index 0 = transparent, so at most 15 visible colors.
OUT   = (0, 0, 85)
LIGHT = (255, 255, 255)
MID   = (170, 170, 170)
DARK  = (85, 85, 85)
RED   = (255, 0, 0)
DRED  = (170, 0, 0)
BLUE  = (0, 85, 255)
GOLD  = (255, 170, 0)
BROWN = (170, 85, 0)
SKIN  = (255, 170, 85)

W = H = 16

def frame(bob, back_leg, front_leg):
    g = [[None] * W for _ in range(H)]

    def rect(x0, y0, x1, y1, c):
        for y in range(y0, y1 + 1):
            for x in range(x0, x1 + 1):
                if 0 <= x < W and 0 <= y < H:
                    g[y][x] = c

    # legs: (x, bottom_y, toe)
    for (x, bottom, toe), shade in ((back_leg, DARK), (front_leg, MID)):
        rect(x, 11, x + 1, bottom, shade)
        rect(x, bottom, x + 1, bottom, BROWN)
        if toe:
            rect(x + 2, bottom, x + 2, bottom, BROWN)

    b = bob
    # plume
    rect(3, 2 + b, 5, 3 + b, RED)
    rect(4, 1 + b, 5, 1 + b, DRED)
    rect(3, 3 + b, 3, 3 + b, DRED)
    # helmet
    rect(5, 3 + b, 10, 7 + b, MID)
    rect(5, 3 + b, 6, 6 + b, LIGHT)
    rect(5, 7 + b, 10, 7 + b, DARK)
    rect(10, 3 + b, 10, 6 + b, DARK)
    # visor slit and face
    rect(8, 5 + b, 10, 5 + b, OUT)
    rect(9, 6 + b, 10, 6 + b, SKIN)
    # torso
    rect(5, 8 + b, 10, 11 + b, MID)
    rect(5, 8 + b, 6, 10 + b, LIGHT)
    rect(5, 11 + b, 10, 11 + b, BROWN)
    rect(8, 11 + b, 8, 11 + b, GOLD)
    # back arm
    rect(4, 9 + b, 4, 10 + b, DARK)
    # shield (front)
    rect(9, 8 + b, 12, 12 + b, BLUE)
    rect(9, 8 + b, 12, 8 + b, LIGHT)
    rect(10, 9 + b, 11, 11 + b, BLUE)
    rect(10, 10 + b, 11, 10 + b, GOLD)
    rect(10, 12 + b, 11, 12 + b, DARK)

    # auto outline
    res = [row[:] for row in g]
    for y in range(H):
        for x in range(W):
            if g[y][x] is None:
                for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                    nx, ny = x + dx, y + dy
                    if 0 <= nx < W and 0 <= ny < H and g[ny][nx] is not None:
                        res[y][x] = OUT
                        break
    return res

frames = [
    frame(0,  (4, 14, False), (9, 14, True)),    # contact, spread
    frame(-1, (6, 14, False), (8, 13, False)),   # passing, front leg lifted
    frame(0,  (9, 14, True),  (4, 14, False)),   # contact, legs swapped
    frame(-1, (5, 13, False), (7, 14, False)),   # passing, back leg lifted
]

img = Image.new("RGBA", (64, 16), (0, 0, 0, 0))
for i, f in enumerate(frames):
    for y in range(H):
        for x in range(W):
            c = f[y][x]
            if c is not None:
                img.putpixel((i * 16 + x, y), c + (255,))

img.save("art.png")
