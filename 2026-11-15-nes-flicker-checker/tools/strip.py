"""Join P6 PPM frames into one PNG grid, left to right then top to bottom, with an
optional crop (stdlib only, MIT).
usage: strip.py out.png a.ppm b.ppm ... [--crop x,y,w,h] [--gap 4] [--cols N]"""
import argparse, struct, zlib

def read_ppm(p):
    d = open(p, "rb").read()
    parts = d.split(b"\n", 3)
    w, h = map(int, parts[1].split())
    return w, h, parts[3]

ap = argparse.ArgumentParser()
ap.add_argument("out"); ap.add_argument("ppm", nargs="+")
ap.add_argument("--crop"); ap.add_argument("--gap", type=int, default=4); ap.add_argument("--cols", type=int, default=0)
a = ap.parse_args()
imgs = [read_ppm(p) for p in a.ppm]
W, H, _ = imgs[0]
cx, cy, cw, ch = map(int, a.crop.split(",")) if a.crop else (0, 0, W, H)
cols = a.cols or len(imgs)
gap = bytes([128, 128, 128]) * a.gap
ow = cw * cols + a.gap * (cols - 1)
rows = []
for r0 in range(0, len(imgs), cols):
    grp = imgs[r0:r0 + cols]
    if r0:
        rows += [b"\x00" + bytes([128, 128, 128]) * ow] * a.gap
    for y in range(cy, cy + ch):
        parts = [pix[(y * w + cx) * 3:(y * w + cx + cw) * 3] for w, h, pix in grp]
        parts += [bytes(cw * 3)] * (cols - len(grp))
        rows.append(b"\x00" + gap.join(parts))
oh = len(rows)
def chunk(t, d):
    return struct.pack(">I", len(d)) + t + d + struct.pack(">I", zlib.crc32(t + d) & 0xffffffff)
png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", ow, oh, 8, 2, 0, 0, 0))
png += chunk(b"IDAT", zlib.compress(b"".join(rows), 9)) + chunk(b"IEND", b"")
open(a.out, "wb").write(png)
print(a.out, ow, oh)
