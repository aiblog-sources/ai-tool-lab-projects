from PIL import Image, ImageDraw

FRAME_W, FRAME_H = 32, 32
FRAMES = 4

# Limited SNES-style palette (RGBA)
CLEAR = (0, 0, 0, 0)
OUTLINE = (28, 28, 40, 255)
STEEL = (160, 170, 186, 255)
STEEL_LIGHT = (214, 222, 232, 255)
STEEL_DARK = (98, 108, 124, 255)
GOLD = (232, 182, 62, 255)
PLUME = (200, 42, 46, 255)
SHIELD_BLUE = (48, 92, 200, 255)
SHIELD_DARK = (28, 56, 130, 255)
BOOT = (112, 66, 34, 255)

# Walk cycle phases: (front_leg_dx, front_lift, back_leg_dx, back_lift, body_bob)
PHASES = [
    (2, 0, -2, 1, 0),    # contact: front leg forward, back leg lifting
    (0, 0, 0, 0, -1),    # passing: legs under body, body rises
    (-2, 1, 2, 0, 0),    # contact (opposite): back leg forward, front lifting
    (0, 0, 0, 0, -1),    # passing: legs under body, body rises
]


def rect(d, x0, y0, x1, y1, color):
    d.rectangle([x0, y0, x1, y1], fill=color)


def outlined(d, x0, y0, x1, y1, fill):
    rect(d, x0 - 1, y0 - 1, x1 + 1, y1 + 1, OUTLINE)
    rect(d, x0, y0, x1, y1, fill)


def draw_leg(d, ox, x0, lift):
    # Leg with boot at the bottom; lift raises the foot off the ground
    rect(d, ox + x0, 22, ox + x0 + 2, 27 - lift, STEEL_DARK)
    rect(d, ox + x0, 28 - lift, ox + x0 + 2, 29 - lift, BOOT)


def draw_knight(d, ox, phase):
    front_dx, front_lift, back_dx, back_lift, bob = PHASES[phase]

    # Back arm (far side), swings opposite the front leg
    back_arm_x = 9 - front_dx
    rect(d, ox + back_arm_x, 14 + bob, ox + back_arm_x + 1, 19 + bob, STEEL_DARK)

    # Legs (drawn before torso so the torso overlaps the hips)
    draw_leg(d, ox, 11 + back_dx, back_lift)
    draw_leg(d, ox, 15 + front_dx, front_lift)

    # Plume on the back of the helmet
    rect(d, ox + 9, 2 + bob, ox + 12, 4 + bob, PLUME)
    rect(d, ox + 11, 5 + bob, ox + 12, 6 + bob, PLUME)

    # Torso with gold trim
    outlined(d, ox + 11, 13 + bob, ox + 19, 21 + bob, STEEL)
    rect(d, ox + 12, 14 + bob, ox + 12, 20 + bob, STEEL_LIGHT)   # highlight
    rect(d, ox + 15, 14 + bob, ox + 15, 20 + bob, GOLD)          # belt-like trim
    rect(d, ox + 11, 19 + bob, ox + 19, 19 + bob, GOLD)

    # Helmet with visor slit
    outlined(d, ox + 11, 6 + bob, ox + 19, 12 + bob, STEEL)
    rect(d, ox + 12, 7 + bob, ox + 12, 8 + bob, STEEL_LIGHT)     # highlight
    rect(d, ox + 15, 9 + bob, ox + 19, 9 + bob, OUTLINE)         # visor slit
    rect(d, ox + 17, 11 + bob, ox + 18, 11 + bob, OUTLINE)       # chin shadow

    # Shield on the front (right) side
    outlined(d, ox + 19, 14 + bob, ox + 23, 21 + bob, SHIELD_BLUE)
    rect(d, ox + 23, 14 + bob, ox + 23, 21 + bob, SHIELD_DARK)
    rect(d, ox + 21, 15 + bob, ox + 21, 20 + bob, GOLD)          # cross vertical
    rect(d, ox + 20, 17 + bob, ox + 22, 17 + bob, GOLD)          # cross horizontal

    # Front arm holding the shield
    front_arm_x = 19 + front_dx // 2
    rect(d, ox + front_arm_x, 15 + bob, ox + front_arm_x + 1, 18 + bob, STEEL)


def main():
    sheet = Image.new("RGBA", (FRAME_W * FRAMES, FRAME_H), CLEAR)
    d = ImageDraw.Draw(sheet)
    for i in range(FRAMES):
        draw_knight(d, i * FRAME_W, i)
    sheet.save("art.png")


if __name__ == "__main__":
    main()
