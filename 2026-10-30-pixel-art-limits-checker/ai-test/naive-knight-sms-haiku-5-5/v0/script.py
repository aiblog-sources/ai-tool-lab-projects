import numpy as np
from PIL import Image

# Palette (RGBA). '.' is transparent.
PAL = {
    'K': (20, 20, 28, 255),     # outline
    'S': (200, 208, 224, 255),  # steel light
    's': (128, 136, 156, 255),  # steel mid
    'G': (216, 168, 48, 255),   # gold belt
    'R': (192, 32, 32, 255),    # red plume
    'H': (32, 80, 192, 255),    # shield blue
    'h': (96, 144, 232, 255),   # shield highlight
    'M': (88, 96, 116, 255),    # greaves
    'B': (106, 58, 26, 255),    # boots
}

# Upper body, facing right, rows 0-11 of a 16x16 frame.
BODY = [
    ".....RRR........",
    "....KRSSK.......",
    "...KSSSSSK......",
    "...KSSKKKKK.....",
    "...KSSSSSK......",
    "....KSSSK.......",
    "...KKKKKKK......",
    "..KSSSSSSSK.....",
    "..KSSSSSSSKHHK..",
    "..KSSSSSSSKHhHK.",
    "..KSSSSSSSKHHK..",
    "..KGGGGGGGK.....",
]

# Walk cycle: (front foot, back foot, body vertical bob)
# Frames alternate contact and passing poses; bob drops the upper body on passing frames.
FRAMES = [
    ((12, 15), (4, 15), 0),   # contact: front foot forward
    ((10, 14), (6, 15), -1),  # passing: back leg lifting
    ((4, 15), (12, 15), 0),   # contact: opposite foot forward
    ((6, 14), (10, 15), -1),  # passing: other leg lifting
]

FRONT_HIP = (9, 11)
BACK_HIP = (6, 11)


def put(canvas, x, y, color):
    if 0 <= x < 16 and 0 <= y < 16:
        canvas[y, x] = PAL[color]


def draw_leg(canvas, hip, foot):
    hx, hy = hip
    fx, fy = foot
    for t in np.linspace(0.0, 1.0, 24):
        x = int(round(hx + (fx - hx) * t))
        y = int(round(hy + (fy - hy) * t))
        put(canvas, x, y, 'M')
        put(canvas, x + 1, y, 'M')
    # Boot: 3 wide, 2 tall, ending at the foot position.
    for dx in (-1, 0, 1):
        for dy in (-1, 0):
            put(canvas, fx + dx, fy + dy, 'B')


def draw_body(canvas, dy):
    for r, row in enumerate(BODY):
        for c, ch in enumerate(row):
            if ch != '.':
                put(canvas, c, r + dy, ch)


def build_frame(front_foot, back_foot, bob):
    canvas = np.zeros((16, 16, 4), dtype=np.uint8)  # fully transparent
    draw_leg(canvas, BACK_HIP, back_foot)
    draw_leg(canvas, FRONT_HIP, front_foot)
    draw_body(canvas, bob)
    return canvas


def main():
    frames = [build_frame(f, b, bob) for f, b, bob in FRAMES]
    sheet = np.concatenate(frames, axis=1)  # 64x16
    Image.fromarray(sheet, 'RGBA').save('art.png')


if __name__ == '__main__':
    main()
