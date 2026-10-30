from PIL import Image

W, H, N = 32, 32, 4
sheet = Image.new("RGBA", (W * N, H), (0, 0, 0, 0))

OUT   = (20, 20, 35, 255)
S_LT  = (200, 210, 225, 255)
S_MD  = (135, 150, 175, 255)
S_DK  = (85, 95, 125, 255)
RED   = (205, 45, 55, 255)
GOLD  = (240, 200, 70, 255)
BLUE  = (55, 95, 205, 255)
BLUE_D = (35, 60, 150, 255)
VISOR = (30, 30, 45, 255)
LEATH = (110, 70, 45, 255)


def rect(im, x0, y0, x1, y1, c):
    for y in range(y0, y1 + 1):
        for x in range(x0, x1 + 1):
            if 0 <= x < W and 0 <= y < H:
                im.putpixel((x, y), c)


def leg(im, hip, top, bottom, dx, col, boot):
    n = bottom - top + 1
    for i in range(n):
        x = hip + round(dx * i / max(n - 1, 1))
        rect(im, x, top + i, x + 2, top + i, col)
    xb = hip + dx
    rect(im, xb, bottom, xb + 4, bottom, boot)  # foot pointing right


def outline(im):
    src = im.copy()
    for y in range(H):
        for x in range(W):
            if src.getpixel((x, y))[3] == 0:
                for nx, ny in ((x+1, y), (x-1, y), (x, y+1), (x, y-1)):
                    if 0 <= nx < W and 0 <= ny < H and src.getpixel((nx, ny))[3]:
                        im.putpixel((x, y), OUT)
                        break


# (far dx, far lift, near dx, near lift, bob, plume offset)
frames = [
    (3, 0, -3, 0, 0, 0),
    (0, 0, 0, 3, -1, 1),
    (-3, 0, 3, 0, 0, 0),
    (0, 3, 0, 0, -1, 1),
]

for f, (fdx, fl, ndx, nl, bob, pl) in enumerate(frames):
    im = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    b = bob

    # far leg (darker)
    leg(im, 16, 22 + b, 28 - fl, fdx, S_DK, LEATH)

    # sword (far hand, held forward and up)
    rect(im, 22, 7 + b, 22, 17 + b, S_LT)
    rect(im, 23, 8 + b, 23, 16 + b, S_MD)
    rect(im, 20, 18 + b, 24, 18 + b, GOLD)
    rect(im, 22, 19 + b, 22, 20 + b, LEATH)
    rect(im, 20, 19 + b, 21, 20 + b, S_MD)  # hand/gauntlet

    # torso
    rect(im, 12, 14 + b, 19, 21 + b, S_MD)
    rect(im, 12, 14 + b, 19, 15 + b, S_LT)
    rect(im, 12, 19 + b, 19, 21 + b, S_DK)
    rect(im, 12, 20 + b, 19, 20 + b, GOLD)

    # near leg
    leg(im, 13, 22 + b, 28 - nl, ndx, S_MD, LEATH)
    rect(im, 13 + 0, 22 + b, 15, 22 + b, S_DK)  # hip shadow

    # shield on the near arm
    rect(im, 14, 16 + b, 19, 22 + b, BLUE)
    rect(im, 14, 16 + b, 19, 16 + b, S_LT)
    rect(im, 14, 17 + b, 14, 22 + b, BLUE_D)
    rect(im, 16, 18 + b, 17, 20 + b, GOLD)
    rect(im, 15, 23 + b, 18, 23 + b, BLUE_D)

    # head / helmet
    rect(im, 12, 6 + b, 19, 13 + b, S_MD)
    rect(im, 12, 6 + b, 19, 7 + b, S_LT)
    rect(im, 12, 12 + b, 19, 13 + b, S_DK)
    rect(im, 15, 9 + b, 19, 10 + b, VISOR)   # visor slit facing right
    rect(im, 16, 12 + b, 16, 12 + b, VISOR)  # breathing hole

    # plume trailing behind (left)
    rect(im, 14, 3 + b, 17, 5 + b, RED)
    rect(im, 11 - pl, 4 + b, 13, 6 + b, RED)
    rect(im, 10 - pl, 6 + b, 12, 7 + b, RED)

    outline(im)
    sheet.paste(im, (f * W, 0))

sheet.save("art.png")
