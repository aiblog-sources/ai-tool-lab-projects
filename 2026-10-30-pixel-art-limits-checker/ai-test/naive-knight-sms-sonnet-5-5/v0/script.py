from PIL import Image

# SMS palette: each channel is one of 0, 85, 170, 255
PAL = {
    '.': (0, 0, 0, 0),
    'K': (0, 0, 0, 255),        # outline
    'W': (255, 255, 255, 255),  # highlight
    'S': (170, 170, 170, 255),  # steel
    'D': (85, 85, 85, 255),     # dark steel
    'R': (255, 0, 0, 255),      # plume
    'r': (170, 0, 0, 255),      # dark plume
    'B': (0, 85, 255, 255),     # shield blue
    'Y': (255, 170, 0, 255),    # gold
}

upper = [
    "................",
    "......KKKK......",
    ".....KWWSSK.....",
    "...RRKWSSSSK....",
    "..rRKSSKKKKK....",
    "....KSSSSSSK....",
    "....KDSSSSK.....",
    "....KSSSSSKBBK..",
    "....KSYYSSKBWBK.",
    "....KSSSSSKBBBK.",
    "....KDSSSDKBBK..",
    ".....KYYYYK.....",
]

legs = {
    'A': [
        ".....KSSSSK.....",
        "....KSK.KSK.....",
        "...KSK...KSK....",
        "..KKK....KKKK...",
    ],
    'B': [
        ".....KSSSSK.....",
        ".....KSKKSK.....",
        ".....KSKKSK.....",
        "....KKKKKKKK....",
    ],
    'C': [
        ".....KSSSSK.....",
        ".....KSKSK......",
        "....KSK.KSK.....",
        "...KKK..KKKK....",
    ],
}

def build(bob, leg):
    rows = []
    if bob:
        rows = upper[1:] + [legs['B'][0]]
        rows = rows[:12]
    else:
        rows = list(upper)
    rows += legs[leg]
    return [r.ljust(16, '.')[:16] for r in rows]

frames = [build(False, 'A'), build(True, 'B'), build(False, 'C'), build(True, 'B')]

img = Image.new("RGBA", (64, 16), (0, 0, 0, 0))
for i, f in enumerate(frames):
    for y, row in enumerate(f):
        for x, c in enumerate(row):
            img.putpixel((i * 16 + x, y), PAL[c])

img.save("art.png")
