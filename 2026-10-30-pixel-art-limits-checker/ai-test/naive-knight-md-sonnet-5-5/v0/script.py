from PIL import Image

# Mega Drive-friendly palette (9-bit colour levels: 0,52,87,116,144,172,206,255)
OUT   = (36 and 0, 0, 0)
OUT   = (0, 0, 52)
STEEL = (172, 172, 206)
STEEL_L = (206, 206, 255)
STEEL_D = (87, 87, 144)
STEEL_DD = (52, 52, 87)
RED   = (206, 52, 52)
RED_D = (144, 0, 52)
BLUE  = (52, 87, 206)
GOLD  = (255, 206, 52)
SKIN  = (255, 172, 116)
BROWN = (116, 52, 0)

W, H, N = 32, 32, 4
sheet = Image.new("RGBA", (W * N, H), (0, 0, 0, 0))


def draw_frame(idx):
    g = {}

    def rect(x0, y0, x1, y1, c):
        for y in range(y0, y1 + 1):
            for x in range(x0, x1 + 1):
                g[(x, y)] = c

    def px(x, y, c):
        g[(x, y)] = c

    bob = [0, -1, 0, -1][idx]
    # leg setup: (far dx, far lift, near dx, near lift)
    legs = [(-3, 0, 3, 0), (0, 1, 0, 0), (3, 0, -3, 0), (0, 0, 0, 1)][idx]
    fdx, flift, ndx, nlift = legs

    def leg(dx, lift, body_c, dark_c, foot_c):
        top = 22 + bob
        bottom = 29 - lift
        n = bottom - top
        for y in range(top, bottom + 1):
            t = (y - top) / n
            x = 13 + round(dx * t)
            rect(x, y, x + 3, y, body_c)
            px(x, y, dark_c)
        # boot
        bx = 13 + dx
        rect(bx, bottom - 1, bx + 5, bottom, foot_c)
        px(bx + 5, bottom, foot_c)

    leg(fdx, flift, STEEL_D, STEEL_DD, BROWN)
    # back arm (far side) behind torso
    rect(11, 16 + bob, 12, 21 + bob, STEEL_D)
    # torso
    rect(12, 15 + bob, 20, 22 + bob, STEEL)
    rect(12, 15 + bob, 20, 15 + bob, STEEL_L)
    rect(12, 15 + bob, 12, 22 + bob, STEEL_D)
    # tabard
    rect(14, 17 + bob, 18, 22 + bob, RED)
    rect(14, 17 + bob, 14, 22 + bob, RED_D)
    # belt
    rect(12, 21 + bob, 20, 21 + bob, BROWN)
    px(16, 21 + bob, GOLD)
    # near leg
    leg(ndx, nlift, STEEL, STEEL_D, BROWN)
    # head / helmet
    rect(12, 6 + bob, 20, 14 + bob, STEEL)
    rect(13, 6 + bob, 19, 6 + bob, STEEL_L)
    rect(12, 7 + bob, 13, 14 + bob, STEEL_D)
    rect(12, 14 + bob, 20, 14 + bob, STEEL_D)
    # visor and face
    rect(16, 9 + bob, 20, 11 + bob, STEEL_DD)
    rect(16, 10 + bob, 20, 10 + bob, SKIN)
    px(18, 10 + bob, OUT)
    # crest line + plume
    rect(15, 5 + bob, 18, 5 + bob, RED)
    rect(10, 6 + bob, 12, 8 + bob, RED)
    px(9, 8 + bob, RED_D)
    rect(10, 9 + bob, 11, 10 + bob, RED_D)
    # shield on chest/front arm
    rect(16, 16 + bob, 21, 22 + bob, BLUE)
    rect(17, 23 + bob, 20, 23 + bob, BLUE)
    rect(16, 16 + bob, 21, 16 + bob, GOLD)
    rect(16, 16 + bob, 16, 22 + bob, GOLD)
    rect(21, 16 + bob, 21, 22 + bob, GOLD)
    rect(18, 18 + bob, 19, 21 + bob, STEEL_L)
    # sword held forward
    sx = 24
    rect(sx, 10 + bob, sx, 18 + bob, STEEL_L)
    px(sx, 9 + bob, STEEL_L)
    rect(sx - 1, 19 + bob, sx + 1, 19 + bob, GOLD)
    rect(sx, 20 + bob, sx, 21 + bob, BROWN)
    rect(22, 20 + bob, 23, 21 + bob, STEEL)  # gauntlet

    # outline pass
    out = {}
    for (x, y) in list(g.keys()):
        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            p = (x + dx, y + dy)
            if p not in g:
                out[p] = OUT
    g.update(out)

    for (x, y), c in g.items():
        if 0 <= x < W and 0 <= y < H:
            sheet.putpixel((idx * W + x, y), c + (255,))


for i in range(N):
    draw_frame(i)

sheet.save("art.png")
