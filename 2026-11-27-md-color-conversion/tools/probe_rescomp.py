#!/usr/bin/env python3
"""Probe how SGDK rescomp turns 8-bit palette entries into Mega Drive colors, and what Genesis Plus GX shows.

Writes 4 indexed PNGs (64 palette entries each, grey r=g=b=v for v = 0..255), builds each with the SGDK Docker
image, reads the palette words rescomp wrote into the ROM's resource file, takes an emulator screenshot with
mdprof, and records (v, CRAM word, level, emulator RGB). Output: results/rescomp-probe.csv. MIT license."""
import csv, os, shutil, subprocess, sys
from pathlib import Path

import numpy as np
from PIL import Image

HERE = Path(__file__).resolve().parent.parent
WORK = Path(os.environ.get("WORK", Path.home() / ".cache/mdcolor")) / "probe"
MDPROF = Path(os.environ.get("MDPROF", Path.home() / ".cache/mdprof"))
SGDK = os.environ.get("SGDK_IMAGE", "ghcr.io/stephane-d/sgdk:latest")
UID = f"{os.getuid()}:{os.getgid()}"


def build_and_shoot(png_maker, tag):
    d = WORK / tag
    shutil.rmtree(d, ignore_errors=True)
    shutil.copytree(HERE / "rom", d)
    png_maker(d / "res/bg.png")
    log = subprocess.run(["docker", "run", "--rm", "--network", "none", "--user", UID, "-v", f"{d}:/src", SGDK],
                         capture_output=True, text=True)
    if log.returncode:
        sys.exit(log.stdout[-2000:] + log.stderr[-2000:])
    subprocess.run(["docker", "run", "--rm", "--network", "none", "--user", UID, "-v", f"{MDPROF}:/m:ro", "-v", f"{d}:/r",
                    "-w", "/m", "gcc:14", "./mdprof", "gpgx/genesis_plus_gx_libretro.so", "/r/out/rom.bin",
                    "/r/out/symbol.txt", "150", "--shot", "140:/r/shot.ppm"], check=True, capture_output=True)
    return d


def read_palette_words(d):
    # SGDK's Palette struct in ROM: u16 length, then a 32-bit pointer to the u16 color words (big-endian)
    rom = (d / "out/rom.bin").read_bytes()
    sym = {l.split()[2]: int(l.split()[0], 16) for l in (d / "out/symbol.txt").read_text().splitlines() if len(l.split()) == 3}
    a = sym["bg_pal"]
    n = int.from_bytes(rom[a:a + 2], "big")
    ptr = int.from_bytes(rom[a + 2:a + 6], "big")
    return [int.from_bytes(rom[ptr + 2 * i:ptr + 2 * i + 2], "big") for i in range(n)]


def main():
    rows = []
    for part in range(4):
        values = list(range(part * 64, part * 64 + 64))

        def make(path, values=values):
            idx = np.zeros((64, 64), dtype=np.uint8)
            for i in range(64):
                y, x = divmod(i, 8)
                idx[y * 8:y * 8 + 8, x * 8:x * 8 + 8] = i
            img = Image.fromarray(idx, mode="P")
            img.putpalette([v for v in values for _ in range(3)])
            img.save(path)

        d = build_and_shoot(make, f"grey{part}")
        words = read_palette_words(d)
        shot = np.array(Image.open(d / "shot.ppm").convert("RGB"))
        for i, v in enumerate(values):
            y, x = divmod(i, 8)
            rgb = shot[y * 8 + 4, x * 8 + 4]
            w = words[i]
            rows.append(dict(v=v, cram=f"{w:04X}", r_level=(w >> 1) & 7, g_level=(w >> 5) & 7, b_level=(w >> 9) & 7,
                             emu_r=int(rgb[0]), emu_g=int(rgb[1]), emu_b=int(rgb[2])))
    out = HERE / "results/rescomp-probe.csv"
    with open(out, "w", newline="") as f:
        wr = csv.DictWriter(f, fieldnames=list(rows[0]))
        wr.writeheader(); wr.writerows(rows)
    print(f"wrote {out} ({len(rows)} rows)")


if __name__ == "__main__":
    main()
