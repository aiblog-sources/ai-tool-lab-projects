"""Writes the test ROM's sprites as indexed PNGs (stdlib only, drawn by code)."""
import pathlib
import struct
import sys
import zlib

PAL = [(255, 0, 255), (224, 224, 224), (224, 96, 0), (0, 96, 224), (32, 32, 32)]


def png(path, rows):
    h, w = len(rows), len(rows[0])
    raw = b"".join(b"\x00" + bytes(r) for r in rows)

    def chunk(t, d):
        return struct.pack(">I", len(d)) + t + d + struct.pack(">I", zlib.crc32(t + d) & 0xffffffff)

    plte = bytes(c for rgb in PAL + [(0, 0, 0)] * (16 - len(PAL)) for c in rgb)
    path.write_bytes(
        b"\x89PNG\r\n\x1a\n"
        + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 3, 0, 0, 0))
        + chunk(b"PLTE", plte)
        + chunk(b"IDAT", zlib.compress(raw, 9))
        + chunk(b"IEND", b"")
    )


def box(n):
    """A solid square with a light outline: every pixel opaque, so any loss shows."""
    return [[1 if x in (0, n - 1) or y in (0, n - 1) else 2 for x in range(n)] for y in range(n)]


def disc(n):
    """A filled circle: the corners are transparent, like most real sprites."""
    r = n / 2
    rows = []
    for y in range(n):
        row = []
        for x in range(n):
            d = ((x + 0.5 - r) ** 2 + (y + 0.5 - r) ** 2) ** 0.5
            row.append(0 if d > r else 4 if d > r - 1.5 else 3)
        rows.append(row)
    return rows


out = pathlib.Path(sys.argv[1])
out.mkdir(parents=True, exist_ok=True)
for n in (8, 16, 32):
    png(out / f"box{n}.png", box(n))
png(out / "disc48.png", disc(48))
