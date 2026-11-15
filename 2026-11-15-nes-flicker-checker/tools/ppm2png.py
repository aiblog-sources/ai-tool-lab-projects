"""Convert the P6 PPM screenshots mdprof writes into PNG (stdlib only)."""
import argparse, pathlib, struct, zlib

def ppm_to_png(src: pathlib.Path, dst: pathlib.Path) -> None:
    data = src.read_bytes()
    parts = data.split(b"\n", 3)
    w, h = map(int, parts[1].split())
    pix = parts[3]
    raw = b"".join(b"\x00" + pix[y * w * 3:(y + 1) * w * 3] for y in range(h))
    def chunk(t, d):
        return struct.pack(">I", len(d)) + t + d + struct.pack(">I", zlib.crc32(t + d) & 0xffffffff)
    png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b"")
    dst.write_bytes(png)

ap = argparse.ArgumentParser()
ap.add_argument("ppm", nargs="+")
ap.add_argument("--out", required=True)
a = ap.parse_args()
for p in map(pathlib.Path, a.ppm):
    ppm_to_png(p, pathlib.Path(a.out) / (p.stem + ".png"))
