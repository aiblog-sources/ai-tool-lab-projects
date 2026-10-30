from PIL import Image

W, H = 32, 32
FRAME_COUNT = 4

# Mega Drive friendly palette: 13 colours plus transparency (fits one 16-colour line)
OUT      = (18, 18, 28, 255)
STEEL_L  = (206, 212, 222, 255)
STEEL_M  = (150, 158, 174, 255)
STEEL_D  = (96, 104, 122, 255)
GOLD     = (226, 184, 64, 255)
GOLD_D   = (158, 118, 34, 255)
RED      = (196, 38, 50, 255)
RED_D    = (126, 18, 30, 255)
LEATHER  = (134, 86, 48, 255)
BOOT     = (86, 52, 28, 255)
VISOR    = (24, 26, 36, 255)
CLEAR    = (0, 0, 0, 0)


def rect(img, x0, y0, x1, y1, color):
    """Fill an inclusive rectangle, clipped to the frame."""
    for y in range(y0, y1 + 1):
        for x in range(x0, x1 + 1):
            if 0 <= x < W and 0 <= y < H:
                img.putpixel((x, y), color)


def add_outline(img):
    """Give every transparent pixel touching the sprite a dark outline."""
    src = img.copy()
    for y in range(H):
        for x in range(W):
            if src.getpixel((x, y))[3] != 0:
                continue
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                nx, ny = x + dx, y + dy
                if 0 <= nx < W and 0 <= ny < H and src.getpixel((nx, ny))[3] > 0:
                    img.putpixel((x, y), OUT)
                    break


def draw_frame(img, f):
    # Walk cycle: contact, passing, contact, passing
    step = [2, 0, -2, 0][f]   # near-leg forward offset
    bob = -1 if f % 2 == 1 else 0  # body rises on passing frames
    b = bob
    far_step = -step           # far leg is opposite the near leg

    # Cape (behind body)
    rect(img, 9, 13 + b, 12, 20 + b, RED)
    rect(img, 9, 13 + b, 9, 20 + b, RED_D)

    # Far arm (swings with far leg, behind torso)
    af = step
    rect(img, 11 + af, 13 + b, 12 + af, 18 + b, STEEL_D)

    # Far leg and boot
    lx = 12 + far_step
    rect(img, lx, 19, lx + 2, 26, STEEL_D)
    rect(img, lx - 1, 27, lx + 3, 29, BOOT)

    # Near leg and boot
    nx = 15 + step
    rect(img, nx, 19, nx + 2, 26, STEEL_M)
    rect(img, nx - 1, 27, nx + 3, 29, BOOT)

    # Torso, gorget, belt
    rect(img, 12, 12 + b, 19, 20 + b, STEEL_M)
    rect(img, 13, 13 + b, 15, 16 + b, STEEL_L)
    rect(img, 13, 11 + b, 18, 11 + b, STEEL_D)
    rect(img, 12, 20 + b, 19, 20 + b, GOLD_D)
    rect(img, 15, 20 + b, 16, 20 + b, GOLD)

    # Shield on the leading (right) side
    rect(img, 20, 13 + b, 23, 20 + b, RED)
    rect(img, 21, 13 + b, 22, 20 + b, GOLD)

    # Near arm swings opposite the near leg, drawn over the shield edge
    ax = -step
    rect(img, 18 + ax, 13 + b, 19 + ax, 17 + b, STEEL_M)
    rect(img, 18 + ax, 18 + b, 19 + ax, 19 + b, LEATHER)

    # Helmet with visor slit
    rect(img, 12, 5 + b, 19, 10 + b, STEEL_M)
    rect(img, 13, 6 + b, 15, 7 + b, STEEL_L)
    rect(img, 16, 8 + b, 19, 9 + b, VISOR)

    # Red plume
    rect(img, 10, 1 + b, 13, 4 + b, RED)
    rect(img, 8, 2 + b, 10, 3 + b, RED_D)


def main():
    sheet = Image.new("RGBA", (W * FRAME_COUNT, H), CLEAR)
    for f in range(FRAME_COUNT):
        frame = Image.new("RGBA", (W, H), CLEAR)
        draw_frame(frame, f)
        add_outline(frame)
        sheet.paste(frame, (f * W, 0))
    sheet.save("art.png")


if __name__ == "__main__":
    main()
