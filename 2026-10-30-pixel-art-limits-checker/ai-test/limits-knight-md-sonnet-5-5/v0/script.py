from PIL import Image, ImageDraw

# Genesis colour levels: 3 bits per channel (round(i*255/7))
L = [round(i * 255 / 7) for i in range(8)]
def c(r, g, b):
    return (L[r], L[g], L[b])

pal = [
    c(7, 0, 7),  # 0 transparent (unused colour)
    c(1, 1, 2),  # 1 outline
    c(6, 6, 7),  # 2 steel light
    c(4, 4, 5),  # 3 steel mid
    c(2, 2, 3),  # 4 steel dark
    c(6, 1, 1),  # 5 plume red
    c(4, 0, 0),  # 6 plume dark
    c(7, 5, 4),  # 7 skin
    c(1, 2, 5),  # 8 shield blue
    c(2, 4, 6),  # 9 shield light
    c(7, 6, 1),  # 10 gold
    c(4, 2, 1),  # 11 brown
    c(2, 1, 0),  # 12 dark brown
    c(7, 7, 7),  # 13 white
    c(0, 0, 0),  # 14 black
    c(5, 5, 6),  # 15 spare steel
]
flat = []
for p in pal:
    flat += list(p)
flat += [0] * (768 - len(flat))

W, H = 128, 32
img = Image.new("P", (W, H), 0)
img.putpalette(flat)
d = ImageDraw.Draw(img)

def R(ox, x0, y0, x1, y1, col):
    d.rectangle([ox + x0, y0, ox + x1, y1], fill=col)

def leg(ox, hx, dx, lift, back):
    main = 4 if back else 3
    boot = 12 if back else 11
    R(ox, hx, 23, hx + 2, 26, main)
    kx = hx + dx // 2
    R(ox, kx, 27, kx + 2, 27, main)
    fx = hx + dx
    if 29 - lift >= 28:
        R(ox, fx, 28, fx + 2, 29 - lift, main)
    R(ox, fx, 30 - lift - 1, fx + 4, 30 - lift, boot)

def frame(ox, phase):
    up = 1 if phase in (1, 3) else 0
    by = -up
    # legs (ground rows fixed)
    if phase == 0:
        leg(ox, 12, -3, 0, True); leg(ox, 16, 2, 0, False)
    elif phase == 1:
        leg(ox, 12, 0, 0, True); leg(ox, 16, 0, 3, False)
    elif phase == 2:
        leg(ox, 12, 2, 0, True); leg(ox, 16, -3, 0, False)
    else:
        leg(ox, 12, 0, 3, True); leg(ox, 16, 0, 0, False)
    # sword (behind)
    d.line([ox + 9, 20 + by, ox + 6, 9 + by], fill=2)
    d.line([ox + 10, 20 + by, ox + 7, 9 + by], fill=13)
    R(ox, 7, 20 + by, 12, 20 + by, 10)
    R(ox, 9, 21 + by, 10, 22 + by, 11)
    # torso
    R(ox, 11, 14 + by, 20, 22 + by, 3)
    R(ox, 11, 14 + by, 20, 15 + by, 2)
    R(ox, 11, 20 + by, 20, 22 + by, 4)
    R(ox, 11, 21 + by, 20, 22 + by, 11)
    R(ox, 15, 21 + by, 16, 22 + by, 10)
    # head / helmet
    R(ox, 11, 5 + by, 20, 13 + by, 3)
    R(ox, 12, 5 + by, 19, 6 + by, 2)
    R(ox, 11, 12 + by, 20, 13 + by, 4)
    R(ox, 17, 9 + by, 20, 10 + by, 14)   # visor slit
    R(ox, 18, 11 + by, 20, 11 + by, 7)   # face
    R(ox, 11, 7 + by, 12, 11 + by, 4)
    # plume
    R(ox, 13, 2 + by, 17, 4 + by, 5)
    R(ox, 9, 3 + by, 12, 6 + by, 5)
    R(ox, 8, 6 + by, 10, 8 + by, 6)
    R(ox, 13, 4 + by, 17, 4 + by, 6)
    # shield (front)
    R(ox, 17, 14 + by, 24, 22 + by, 8)
    R(ox, 17, 14 + by, 24, 15 + by, 9)
    R(ox, 18, 16 + by, 18, 21 + by, 9)
    R(ox, 20, 17 + by, 22, 19 + by, 10)
    R(ox, 21, 18 + by, 21, 18 + by, 13)
    R(ox, 19, 22 + by, 22, 23 + by, 8)
    # arm
    R(ox, 13, 16 + by, 15, 19 + by, 2)

for i in range(4):
    frame(i * 32, i)

# auto outline
px = img.load()
out = []
for y in range(H):
    for x in range(W):
        if px[x, y] == 0:
            for nx, ny in ((x+1, y), (x-1, y), (x, y+1), (x, y-1)):
                if 0 <= nx < W and 0 <= ny < H and px[nx, ny] not in (0, 1):
                    out.append((x, y)); break
for x, y in out:
    px[x, y] = 1

img.save("art.png", transparency=0)
