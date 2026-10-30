#!/usr/bin/env python3
"""Synthetic images with a known number of violations; pixcheck must report exactly that number.

Run: python3 tests/test_pixcheck.py   (writes the images to tests/cases/ so they can be inspected)"""
import os
import sys

import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "pixcheck"))
import pixcheck  # noqa: E402

CASES = os.path.join(HERE, "cases")
os.makedirs(CASES, exist_ok=True)


def grid_color(levels, k):
    """k-th distinct on-grid color for a console with `levels` per channel (skips black, the backdrop)."""
    k += 1
    step = 255 / (levels - 1)
    r, g, b = k % levels, (k // levels) % levels, (k // levels // levels) % levels
    return (round(r * step), round(g * step), round(b * step))


def save(name, arr):
    path = os.path.join(CASES, name)
    Image.fromarray(arr.astype(np.uint8), "RGBA" if arr.shape[2] == 4 else "RGB").save(path)
    return path


def tile_fill(arr, tx, ty, colors):
    """Paint tile (tx, ty) with the given colors, one pixel row pattern per color, so each color is used."""
    for i in range(64):
        arr[ty * 8 + i // 8, tx * 8 + i % 8, :3] = colors[i % len(colors)]


def run(console, arr, name, sprite=None):
    report, _, _ = pixcheck.check(console, save(name, arr), sprite)
    return report


results = []


def expect(label, cond, detail):
    results.append((label, bool(cond)))
    print(("ok   " if cond else "FAIL ") + label + ("" if cond else f"  ({detail})"))


# 1. Mega Drive: 4 palettes x 15 colors + black backdrop, every tile inside one palette -> PASS
md = np.zeros((224, 320, 3))
pals = [[grid_color(8, p * 15 + i) for i in range(15)] for p in range(4)]
for ty in range(28):
    for tx in range(40):
        p = (tx + ty) % 4
        k = (tx * 3 + ty) % 15
        tile_fill(md, tx, ty, [(0, 0, 0)] + pals[p][k:] + pals[p][:k])
r = run("md", md, "md-valid.png")
expect("md valid image passes", r["ok"], r["errors"])
expect("md valid: 61 console colors", r["info"]["console_colors"] == 61, r["info"]["console_colors"])

# 2. Mega Drive: three tiles with 16 colors besides the backdrop -> 3 tiles over
bad = md.copy()
for tx in (5, 17, 30):
    tile_fill(bad, tx, 10, [(0, 0, 0)] + pals[0] + [pals[1][0]])
r = run("md", bad, "md-16-color-tiles.png")
expect("md: 3 tiles over 15 colors", len(r["bad_areas"]) == 3 and not r["ok"], r["bad_areas"])

# 3. Mega Drive: five disjoint palettes worth of tiles -> cannot pack, total over 60
five = np.zeros((40, 40, 3))
sets = [[grid_color(8, p * 15 + i) for i in range(15)] for p in range(5)]
for p in range(5):
    tile_fill(five, p, 0, sets[p])
r = run("md", five, "md-five-palettes.png")
expect("md: 5 x 15 disjoint colors leave 1 tile unplaced", len(r["unplaced"]) == 1, r["unplaced"])
expect("md: 75 colors reported over 60", any("75 colors" in e for e in r["errors"]), r["errors"])

# 4. NES: two 8x8 tiles with 3 colors each, in the same 16x16 attribute area, 5 colors together
# NES colors are taken as given; any distinct RGB values stand for palette entries here
pal = {0x0F: (0, 0, 0), 0x16: (248, 56, 0), 0x27: (252, 160, 68), 0x30: (252, 252, 252), 0x12: (0, 88, 248),
       0x2A: (88, 216, 84)}
c = [pal[i] for i in (0x16, 0x27, 0x30, 0x12, 0x2A)]
nes = np.zeros((32, 32, 3))
nes[:, :] = pal[0x0F]
tile_fill(nes, 0, 0, [pal[0x0F], c[0], c[1]])
tile_fill(nes, 1, 0, [pal[0x0F], c[2], c[3]])
tile_fill(nes, 2, 2, [pal[0x0F], c[4]])
r = run("nes", nes, "nes-attribute-clash.png")
expect("nes: per-tile-legal tiles clash in one 16x16 area", len(r["bad_areas"]) == 1 and r["bad_areas"][0]["x"] == 0,
       r["bad_areas"])

# 5. NES sprite: one 8x8 tile with 4 opaque colors in a 16x16 sprite
spr = np.zeros((16, 16, 4))
tile_fill(spr, 0, 0, [pal[i] for i in (0x16, 0x27, 0x30, 0x12)])
tile_fill(spr, 1, 1, [pal[i] for i in (0x16, 0x27)])
spr[:16, :16, 3] = 0
spr[:8, :8, 3] = 255
spr[8:, 8:, 3] = 255
r = run("nes", spr, "nes-sprite-4-colors.png", sprite=(16, 16))
expect("nes sprite: 1 tile over 3 colors", len(r["bad_areas"]) == 1, r["bad_areas"])

# 6. Master System: tiles of 12 colors each, 3 disjoint sets (36 colors) -> 2 palettes of 16 cannot hold them
sms = np.zeros((8, 24, 3))
for t in range(3):
    tile_fill(sms, t, 0, [grid_color(4, t * 12 + i) for i in range(12)])
r = run("sms", sms, "sms-36-colors.png")
expect("sms: 3 x 12 disjoint colors leave 1 tile unplaced", len(r["unplaced"]) == 1, r["unplaced"])

# 7. Off-grid colors: 5 colors 20 away from the Mega Drive grid, 3 exactly on it
off = np.zeros((8, 64, 3))
for t in range(8):
    col = grid_color(8, t)
    if t < 5:
        col = (min(255, col[0] + 20) if col[0] < 200 else col[0] - 20, col[1], col[2])
    tile_fill(off, t, 0, [col])
r = run("md", off, "md-off-grid.png")
expect("md: 5 off-grid colors", r["info"]["off_grid_colors"] == 5, r["info"])

# 7b. NES colors are not judged without a palette file; with one, off-palette colors are counted
r = run("nes", nes, "nes-attribute-clash.png")
expect("nes: no color warnings without a palette file", r["info"]["off_grid_colors"] == 0, r["info"])
pal_file = os.path.join(CASES, "test.pal")
with open(pal_file, "wb") as f:
    f.write(bytes(ch for c in (list(pal.values()) + [(0, 0, 0)] * 58)[:64] for ch in c))
nes2 = nes.copy()
nes2[30, 30] = (120, 120, 120)          # far from every entry in the test palette
report, _, _ = pixcheck.check("nes", save("nes-off-palette.png", nes2), None, pixcheck.read_pal(pal_file))
expect("nes with palette file: 1 off-palette color", report["info"]["off_grid_colors"] == 1, report["info"])

# 8. Tile budget: 257 different NES tiles fail; mirrored copies on the Mega Drive count once
rng = np.random.default_rng(1)
nest = np.zeros((8 * 17, 8 * 16, 3))
nest[:, :] = pal[0x0F]
for t in range(257):
    tx, ty = t % 16, t // 16
    bits = rng.integers(0, 2, (8, 8))
    nest[ty * 8:ty * 8 + 8, tx * 8:tx * 8 + 8] = np.where(bits[..., None], pal[0x30], pal[0x0F])
r = run("nes", nest, "nes-257-tiles.png")
expect("nes: 257 unique tiles over 256", r["info"]["unique_tiles"] >= 257 and not r["ok"], r["info"])

mir = np.zeros((8, 32, 3))
base = np.where(rng.integers(0, 2, (8, 8))[..., None], (255, 255, 255), (0, 0, 0))
mir[:, 0:8], mir[:, 8:16], mir[:, 16:24], mir[:, 24:32] = base, base[:, ::-1], base[::-1], base[::-1, ::-1]
r = run("md", mir, "md-mirrored.png")
expect("md: 4 mirrored tiles count as 1", r["info"]["unique_tiles"] == 1, r["info"]["unique_tiles"])
r = run("nes", mir, "nes-mirrored.png")
expect("nes background: 4 mirrored tiles count as 4", r["info"]["unique_tiles"] == 4, r["info"]["unique_tiles"])

failed = [label for label, ok in results if not ok]
print(f"{len(results) - len(failed)}/{len(results)} checks passed")
sys.exit(1 if failed else 0)
