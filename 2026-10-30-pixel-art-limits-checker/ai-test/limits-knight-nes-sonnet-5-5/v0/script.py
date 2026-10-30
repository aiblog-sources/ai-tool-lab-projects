from PIL import Image

# NES palette colors (sprite palette: transparent + 3 colors)
K = (0, 0, 0, 255)        # $0F black
W = (252, 252, 252, 255)  # $30 white
R = (216, 40, 0, 255)     # $16 red
T = (0, 0, 0, 0)          # transparent

img = Image.new("RGBA", (64, 16), T)


def frame(fi, bob, back_x, front_x, front_lift):
    ox = fi * 16

    def px(x, y, c):
        if 0 <= x < 16 and 0 <= y < 16:
            img.putpixel((ox + x, y), c)

    def rect(x0, y0, x1, y1, c):
        for y in range(y0, y1 + 1):
            for x in range(x0, x1 + 1):
                px(x, y, c)

    b = bob
    # plume
    rect(3, 1 + b, 4, 3 + b, R)
    px(5, 1 + b, R)
    # helmet
    rect(5, 1 + b, 11, 7 + b, K)
    rect(6, 2 + b, 10, 6 + b, W)
    rect(8, 4 + b, 10, 4 + b, K)  # visor slit
    rect(6, 5 + b, 7, 6 + b, W)
    # torso
    rect(5, 8 + b, 10, 12 + b, K)
    rect(6, 9 + b, 9, 11 + b, W)
    rect(6, 11 + b, 9, 11 + b, R)  # belt
    # shield (front)
    rect(10, 8 + b, 13, 12 + b, K)
    rect(11, 9 + b, 12, 11 + b, R)
    # back leg
    for y in range(13 + b, 15):
        rect(back_x, y, back_x + 1, y, W)
    rect(back_x - 1, 15, back_x + 1, 15, R)
    # front leg
    top = 13 + b
    bottom = 14 - front_lift
    for y in range(top, bottom + 1):
        rect(front_x, y, front_x + 1, y, W)
    fy = 15 - front_lift
    rect(front_x, fy, front_x + 2, fy, K)


frame(0, 1, 5, 9, 0)   # stride
frame(1, 0, 7, 7, 1)   # passing, front foot lifted
frame(2, 1, 9, 5, 0)   # opposite stride
frame(3, 0, 7, 8, 1)   # passing, other

img.save("art.png")
