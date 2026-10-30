#!/usr/bin/env python3
"""pixcheck: check a PNG against the color and tile limits of the NES, Master System, SNES or Mega Drive.

Usage:
  pixcheck.py CONSOLE IMAGE.png [--sprite WxH] [--json out.json] [--overlay out.png] [--export out.png]

CONSOLE is nes, sms, snes or md. Without --sprite the image is checked as a background (tilemap); with --sprite it
is a sheet of sprite frames of that size. Exit code 0 when every hard limit holds, 1 when one breaks, 2 on bad input.
--export (Mega Drive backgrounds only) writes an indexed PNG that SGDK's rescomp accepts as an IMAGE resource.
MIT license.
"""
import argparse
import json
import math
import random
import sys
import time

import numpy as np
from PIL import Image, ImageDraw

# Background: `area` is the square that shares one palette, `colors` how many colors one palette gives that area
# (not counting a shared backdrop), `palettes` how many palettes backgrounds can use, `backdrop` whether a color 0
# shared by every palette exists, `flips` whether the tilemap can flip a tile (so mirrored tiles are free),
# `tiles` the tile budget. Sprites: `per` is what one palette covers (an 8x8 tile on the NES, a whole sprite elsewhere).
CONSOLES = {
    "nes": {
        "name": "NES", "levels": None, "screen": (256, 240),
        "bg": {"area": 16, "colors": 3, "palettes": 4, "backdrop": True, "flips": False, "tiles": 256},
        "spr": {"per": "tile", "colors": 3, "palettes": 4, "flips": True, "tiles": 256, "line_sprites": 8},
    },
    "sms": {
        "name": "Master System", "levels": 4, "screen": (256, 192),
        "bg": {"area": 8, "colors": 16, "palettes": 2, "backdrop": False, "flips": True, "tiles": 448},
        "spr": {"per": "frame", "colors": 15, "palettes": 1, "flips": False, "tiles": 448, "line_sprites": 8},
    },
    "snes": {
        "name": "SNES (mode 1, 4bpp layer)", "levels": 32, "screen": (256, 224),
        "bg": {"area": 8, "colors": 15, "palettes": 8, "backdrop": True, "flips": True, "tiles": 1024},
        "spr": {"per": "frame", "colors": 15, "palettes": 8, "flips": True, "tiles": 512, "line_sprites": 32},
    },
    "md": {
        "name": "Mega Drive", "levels": 8, "screen": (320, 224),
        # tiles: SGDK 2.09's TILE_USER_LENGTH with its default plane layout, measured with mdrom/ (1,425)
        "bg": {"area": 8, "colors": 15, "palettes": 4, "backdrop": True, "flips": True, "tiles": 1425},
        "spr": {"per": "frame", "colors": 15, "palettes": 4, "flips": True, "tiles": 1425, "line_sprites": 20},
    },
}

OFF_GRID = 8      # a channel more than this far from the console's nearest level counts as off-grid
NES_FAR = 24      # with --nes-palette: RGB distance above which a color is "not in that palette"


def read_pal(path):
    """An emulator .pal file: 64 (or 512) RGB triplets. Only the first 64 are used."""
    data = open(path, "rb").read()
    if len(data) < 192:
        raise ValueError(f"{path}: a .pal file needs at least 64 RGB triplets (192 bytes)")
    return [tuple(data[i:i + 3]) for i in range(0, 192, 3)]


def snap_colors(colors, console, nes_pal=None):
    """Map each source RGB to the nearest color the console can show. Returns {src: (dst, error)}.

    The NES has no RGB palette of its own (emulators and TVs differ), so NES colors are taken as given unless an
    emulator .pal file is passed."""
    levels = CONSOLES[console]["levels"]
    result = {}
    if levels is None:
        if nes_pal is None:
            return {c: (c, 0.0) for c in colors}
        entries = sorted(set(nes_pal))
        arr = np.array(entries, dtype=float)
        for c in colors:
            d = np.sqrt(((arr - np.array(c, dtype=float)) ** 2).sum(axis=1))
            k = int(d.argmin())
            result[c] = (entries[k], float(d[k]))
        return result
    grid = [round(k * 255 / (levels - 1)) for k in range(levels)]
    for c in colors:
        dst = tuple(min(grid, key=lambda g: abs(g - ch)) for ch in c)
        err = max(abs(a - b) for a, b in zip(c, dst))
        result[c] = (dst, float(err))
    return result


def load(path):
    img = Image.open(path).convert("RGBA")
    a = np.array(img)
    opaque = a[:, :, 3] >= 128
    rgb = a[:, :, :3].astype(np.uint32)
    packed = (rgb[:, :, 0] << 16) | (rgb[:, :, 1] << 8) | rgb[:, :, 2]
    packed[~opaque] = 0xFFFFFFFF          # transparent marker
    return packed


def unpack(v):
    return ((v >> 16) & 255, (v >> 8) & 255, v & 255)


def pack(c):
    return (c[0] << 16) | (c[1] << 8) | c[2]


TRANSPARENT = 0xFFFFFFFF


EXACT_SECONDS = 120


def exact_pack(sets, capacity, count, seconds):
    """Backtracking search over the maximal color sets. Returns (palettes, "exact") when a packing exists,
    (None, "none") when it proves there is none, (None, "timeout") when it runs out of time."""
    uniq = {frozenset(s) for s in sets if s}
    maximal = sorted([s for s in uniq if not any(s < t for t in uniq)], key=lambda s: (-len(s), sorted(s)))
    if any(len(s) > capacity for s in maximal):
        return None, "none"
    suffix = [frozenset()] * (len(maximal) + 1)
    for i in range(len(maximal) - 1, -1, -1):
        suffix[i] = suffix[i + 1] | maximal[i]
    deadline = time.monotonic() + seconds
    sys.setrecursionlimit(max(10000, 4 * len(maximal)))

    def solve(i, bins):
        if time.monotonic() > deadline:
            raise TimeoutError
        if i == len(maximal):
            return bins
        used = frozenset().union(*bins) if bins else frozenset()
        if len(suffix[i] - used) > count * capacity - sum(len(b) for b in bins):
            return None          # not enough free slots left for the colors still to place
        s = maximal[i]
        tried = set()
        for k, b in enumerate(bins):
            u = b | s
            if len(u) <= capacity and u not in tried:
                tried.add(u)
                r = solve(i + 1, bins[:k] + [u] + bins[k + 1:])
                if r:
                    return r
        if len(bins) < count:
            return solve(i + 1, bins + [s])
        return None

    try:
        r = solve(0, [])
    except TimeoutError:
        return None, "timeout"
    return ([set(b) for b in r], "exact") if r else (None, "none")


def pack_palettes(sets, capacity, count, tries=300, seed=1, exact_seconds=None):
    """Pack color sets into `count` palettes of `capacity` colors: greedy with random restarts first, then an
    exact search when the greedy one leaves sets out (greedy alone gave a false alarm on a real image).

    Returns (palettes, unplaced, method) with method "greedy", "exact" (exact search found a packing), "proven"
    (exact search proved there is none) or "unknown" (exact search ran out of time)."""
    uniq = sorted({frozenset(s) for s in sets if s}, key=lambda s: (-len(s), sorted(s)))
    rng = random.Random(seed)
    best = None
    for attempt in range(tries):
        order = list(uniq)
        if attempt:
            # keep big sets first, shuffle within equal sizes and occasionally swap neighbours
            rng.shuffle(order)
            order.sort(key=lambda s: -len(s) + rng.random() * (1.5 if attempt % 3 == 0 else 0.0))
        pals, left = [], []
        for s in order:
            if len(s) > capacity:
                left.append(s)
                continue
            if any(s <= p for p in pals):
                continue
            fit = [p for p in pals if len(p | s) <= capacity]
            if fit:
                p = max(fit, key=lambda p: (len(p & s), -len(p)))
                p |= s
            elif len(pals) < count:
                pals.append(set(s))
            else:
                left.append(s)
        score = (len(left), len(pals))
        if best is None or score < best[0]:
            best = (score, pals, left)
        if not left and attempt > 20:
            break
    pals, left = best[1], best[2]
    if left and all(len(x) <= capacity for x in left):
        found, status = exact_pack(sets, capacity, count, EXACT_SECONDS if exact_seconds is None else exact_seconds)
        if found:
            return found, [], "exact"
        return pals, left, {"none": "proven", "timeout": "unknown"}[status]
    return pals, left, "greedy"


def packing_note(method, pals):
    return {"greedy": f"greedy packing, {len(pals)} palettes used",
            "proven": "an exact search proved no packing exists",
            "unknown": f"none found by greedy packing or {EXACT_SECONDS} s of exact search, so this is not proven"}[method]


def tile_key(block, flips):
    keys = [block.tobytes()]
    if flips:
        keys += [block[:, ::-1].tobytes(), block[::-1, :].tobytes(), block[::-1, ::-1].tobytes()]
    return min(keys)


def check(console, path, sprite=None, nes_pal=None):
    spec = CONSOLES[console]
    px = load(path)
    h, w = px.shape
    errors, warnings, info = [], [], {}
    if w % 8 or h % 8:
        errors.append(f"size {w}x{h} is not a multiple of 8 (tiles are 8x8)")
        px = np.pad(px, ((0, (-h) % 8), (0, (-w) % 8)), constant_values=TRANSPARENT)
        h, w = px.shape

    src_colors = [unpack(int(v)) for v in np.unique(px) if v != TRANSPARENT]
    snap = snap_colors(src_colors, console, nes_pal)
    lut = {pack(s): pack(d) for s, (d, _) in snap.items()}
    lut[TRANSPARENT] = TRANSPARENT
    keys = np.array(list(lut.keys()), dtype=np.uint32)
    vals = np.array([lut[int(k)] for k in keys], dtype=np.uint32)
    order = np.argsort(keys)
    con = vals[order][np.searchsorted(keys[order], px)]
    con_colors = sorted({d for d, _ in snap.values()})
    far_limit = NES_FAR if spec["levels"] is None else OFF_GRID
    far = sorted((e, s, d) for s, (d, e) in snap.items() if e > far_limit)
    info.update(source_colors=len(src_colors), console_colors=len(con_colors),
                off_grid_colors=len(far), max_snap_error=round(max([e for _, e in snap.values()] or [0]), 1))
    if far:
        what = "not close to any color of the NES palette file" if spec["levels"] is None else "off the console's color grid"
        ex = ", ".join("#%02x%02x%02x->#%02x%02x%02x" % (s + d) for e, s, d in far[-3:][::-1])
        warnings.append(f"{len(far)} of {len(src_colors)} colors are {what}; they will shift when converted ({ex})")
    if len(con_colors) < len(src_colors):
        warnings.append(f"{len(src_colors)} source colors collapse to {len(con_colors)} console colors")

    bad_areas, unplaced_areas = [], []
    if sprite is None:
        b = spec["bg"]
        sw, sh = spec["screen"]
        info["mode"] = "background"
        if w > 512 or h > 512:
            warnings.append(f"{w}x{h} is bigger than a hardware tilemap (512x512 at most); it needs streaming")
        a = b["area"]
        if w % a or h % a:
            errors.append(f"size {w}x{h} is not a multiple of the {a}x{a} palette area")
        areas = {}
        for y in range(0, h, a):
            for x in range(0, w, a):
                blk = con[y:y + a, x:x + a]
                areas[(x, y)] = {int(v) for v in np.unique(blk) if v != TRANSPARENT}
        backdrop = None
        cands = [None]
        if b["backdrop"]:
            counts = {}
            for s in areas.values():
                for c in s:
                    counts[c] = counts.get(c, 0) + 1
            cands = sorted(counts, key=lambda c: -counts[c])[:6] or [None]
        best = None
        for cand in cands:
            sets = {k: (s - {cand}) for k, s in areas.items()}
            pals, left, method = pack_palettes(list(sets.values()), b["colors"], b["palettes"], exact_seconds=0)
            over = [k for k, s in sets.items() if len(s) > b["colors"]]
            score = (len(over), len(left))
            if best is None or score < best[0]:
                best = (score, cand, sets, pals, left, over)
        _, backdrop, sets, pals, left, over = best
        method = "greedy"
        if left and not over:      # only the best backdrop gets the (slow) exact search
            pals, left, method = pack_palettes(list(sets.values()), b["colors"], b["palettes"])
        limit = f"{b['colors']} colors" + (" plus the shared backdrop" if b["backdrop"] else "")
        unit = "tile" if a == 8 else f"{a}x{a} attribute area"
        for k in sorted(over, key=lambda k: (k[1], k[0])):
            bad_areas.append({"x": k[0], "y": k[1], "colors": len(sets[k])})
        if over:
            worst = max(bad_areas, key=lambda r: r["colors"])
            errors.append(f"{len(over)} of {len(areas)} {unit}s use more than {limit} "
                          f"(worst: x={worst['x']} y={worst['y']} with {worst['colors']}"
                          + (" plus backdrop" if b["backdrop"] else "") + ")")
        leftset = {frozenset(s) for s in left if len(s) <= b["colors"]}
        for k, s in sets.items():
            if frozenset(s) in leftset:
                unplaced_areas.append({"x": k[0], "y": k[1], "colors": len(s)})
        if unplaced_areas:
            errors.append(f"the {unit} color sets do not fit in {b['palettes']} palettes of {b['colors']} colors: "
                          f"{len(unplaced_areas)} {unit}s left without a palette ({packing_note(method, pals)})")
        total = len({c for s in sets.values() for c in s})
        info.update(packing=method, areas=len(areas), area_size=a, backdrop=("#%06x" % backdrop) if backdrop is not None else None,
                    palettes_used=len(pals), palettes_allowed=b["palettes"], colors_per_palette=b["colors"],
                    colors_without_backdrop=total)
        if total > b["colors"] * b["palettes"]:
            errors.append(f"{total} colors (without backdrop) is more than {b['palettes']} x {b['colors']} = "
                          f"{b['palettes'] * b['colors']} the backgrounds can hold at once")
        uniq = {tile_key(con[y:y + 8, x:x + 8], b["flips"]) for y in range(0, h, 8) for x in range(0, w, 8)}
        info.update(tiles=(w // 8) * (h // 8), unique_tiles=len(uniq), tile_budget=b["tiles"],
                    flips_counted=b["flips"])
        if len(uniq) > b["tiles"]:
            errors.append(f"{len(uniq)} unique 8x8 tiles, budget {b['tiles']}"
                          + ("" if b["flips"] else " (no tile flipping on NES backgrounds)"))
        info["palettes"] = [["#%06x" % c for c in sorted(p)] for p in pals]
        result_pals = (backdrop, pals, sets)
    else:
        s = spec["spr"]
        fw, fh = sprite
        info["mode"] = f"sprites {fw}x{fh}"
        if w % fw or h % fh:
            errors.append(f"sheet {w}x{h} is not a whole number of {fw}x{fh} frames")
        if fw % 8 or fh % 8:
            errors.append(f"frame {fw}x{fh} is not made of 8x8 tiles")
        a = 8 if s["per"] == "tile" else None
        units = {}
        for fy in range(0, h - fh + 1, fh):
            for fx in range(0, w - fw + 1, fw):
                frame = con[fy:fy + fh, fx:fx + fw]
                if s["per"] == "tile":
                    for y in range(0, fh, 8):
                        for x in range(0, fw, 8):
                            units[(fx + x, fy + y)] = {int(v) for v in np.unique(frame[y:y + 8, x:x + 8])
                                                       if v != TRANSPARENT}
                else:
                    units[(fx, fy)] = {int(v) for v in np.unique(frame) if v != TRANSPARENT}
        over = [k for k, u in units.items() if len(u) > s["colors"]]
        unit = "8x8 sprite tile" if s["per"] == "tile" else "frame"
        for k in over:
            bad_areas.append({"x": k[0], "y": k[1], "colors": len(units[k]), "w": a or fw, "h": a or fh})
        if over:
            worst = max(bad_areas, key=lambda r: r["colors"])
            errors.append(f"{len(over)} of {len(units)} {unit}s use more than {s['colors']} colors + transparent "
                          f"(worst: x={worst['x']} y={worst['y']} with {worst['colors']})")
        pals, left, method = pack_palettes(list(units.values()), s["colors"], s["palettes"])
        info["packing"] = method
        leftset = {frozenset(u) for u in left if len(u) <= s["colors"]}
        for k, u in units.items():
            if frozenset(u) in leftset:
                unplaced_areas.append({"x": k[0], "y": k[1], "colors": len(u), "w": a or fw, "h": a or fh})
        if unplaced_areas:
            errors.append(f"the {unit} color sets do not fit in {s['palettes']} sprite palette"
                          f"{'s' if s['palettes'] > 1 else ''} of {s['colors']} colors: {len(unplaced_areas)} "
                          f"{unit}s left without a palette ({packing_note(method, pals)})")
        # hardware sprites one frame needs on its busiest line, counting 8-pixel-wide columns with any pixel
        worst_line = 0
        for fy in range(0, h - fh + 1, fh):
            for fx in range(0, w - fw + 1, fw):
                frame = con[fy:fy + fh, fx:fx + fw] != TRANSPARENT
                cols = frame.reshape(fh, fw // 8, 8).any(axis=2).sum(axis=1)
                worst_line = max(worst_line, int(cols.max()))
        info.update(frames=len(units) if s["per"] == "frame" else (w // fw) * (h // fh),
                    palettes_used=len(pals), palettes_allowed=s["palettes"], colors_per_palette=s["colors"],
                    widest_line_8px_sprites=worst_line, line_sprite_limit=s["line_sprites"])
        if worst_line > s["line_sprites"]:
            warnings.append(f"one frame needs {worst_line} 8-pixel sprites on one line; the limit is "
                            f"{s['line_sprites']} for everything on that line")
        uniq = {tile_key(con[y:y + 8, x:x + 8], s["flips"]) for y in range(0, h, 8) for x in range(0, w, 8)
                if (con[y:y + 8, x:x + 8] != TRANSPARENT).any()}
        info.update(unique_tiles=len(uniq), tile_budget=s["tiles"])
        if len(uniq) > s["tiles"]:
            errors.append(f"{len(uniq)} unique sprite tiles, budget {s['tiles']}")
        info["palettes"] = [["#%06x" % c for c in sorted(p)] for p in pals]
        result_pals = None
    return {"console": console, "file": path, "size": [w, h], "ok": not errors, "errors": errors,
            "warnings": warnings, "info": info, "bad_areas": bad_areas, "unplaced": unplaced_areas}, con, result_pals


def overlay(path, report, out, scale=3):
    img = Image.open(path).convert("RGBA")
    bg = Image.new("RGBA", img.size, (128, 128, 128, 255))
    img = Image.alpha_composite(bg, img).convert("RGB").resize((img.width * scale, img.height * scale),
                                                              Image.NEAREST)
    d = ImageDraw.Draw(img)
    a = report["info"].get("area_size", 8)
    for color, rows in (((255, 140, 0), report["unplaced"]), ((230, 0, 0), report["bad_areas"])):
        for r in rows:
            rw, rh = r.get("w", a), r.get("h", a)
            d.rectangle([r["x"] * scale, r["y"] * scale, (r["x"] + rw) * scale - 1, (r["y"] + rh) * scale - 1],
                        outline=color, width=2)
    img.save(out)


def export_md(con, pals_info, out):
    """Indexed PNG for SGDK rescomp: pixel index = palette * 16 + color, color 0 of every palette = backdrop."""
    backdrop, pals, sets = pals_info
    h, w = con.shape
    palette = [(0, 0, 0)] * 64
    slots = []
    for p, cols in enumerate(pals):
        cols = sorted(cols)
        palette[p * 16] = unpack(backdrop) if backdrop is not None else (0, 0, 0)
        for i, c in enumerate(cols):
            palette[p * 16 + 1 + i] = unpack(c)
        slots.append({c: p * 16 + 1 + i for i, c in enumerate(cols)})
    idx = np.zeros((h, w), dtype=np.uint8)
    for y in range(0, h, 8):
        for x in range(0, w, 8):
            s = sets[(x, y)]
            p = next((i for i, pal in enumerate(pals) if s <= pal), 0)
            blk = con[y:y + 8, x:x + 8]
            for yy in range(8):
                for xx in range(8):
                    v = int(blk[yy, xx])
                    idx[y + yy, x + xx] = p * 16 if (v == backdrop or v == TRANSPARENT) else slots[p][v]
    img = Image.fromarray(idx, mode="P")
    img.putpalette([ch for c in palette for ch in c])
    img.save(out)


def text_report(r):
    i = r["info"]
    lines = [f"pixcheck {r['console']} {i['mode']} {r['file']} {r['size'][0]}x{r['size'][1]}: "
             + ("PASS" if r["ok"] else "FAIL")]
    lines += [f"  error: {e}" for e in r["errors"]]
    lines += [f"  warning: {wn}" for wn in r["warnings"]]
    keys = ["source_colors", "console_colors", "backdrop", "packing", "palettes_used", "palettes_allowed", "colors_per_palette",
            "unique_tiles", "tile_budget", "widest_line_8px_sprites", "line_sprite_limit"]
    lines.append("  " + ", ".join(f"{k}={i[k]}" for k in keys if k in i))
    shown = r["bad_areas"][:8]
    if shown:
        lines.append("  over the color limit at (x,y): " + "; ".join(
            f"({b['x']},{b['y']}) {b['colors']} colors" for b in shown)
            + (f"; ... {len(r['bad_areas']) - 8} more" if len(r["bad_areas"]) > 8 else ""))
    if r["unplaced"]:
        lines.append("  no palette for (x,y): " + "; ".join(f"({b['x']},{b['y']})" for b in r["unplaced"][:8])
                     + (f"; ... {len(r['unplaced']) - 8} more" if len(r["unplaced"]) > 8 else ""))
    return "\n".join(lines)


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("console", choices=sorted(CONSOLES))
    ap.add_argument("image")
    ap.add_argument("--sprite", help="frame size WxH: check a sprite sheet instead of a background")
    ap.add_argument("--json")
    ap.add_argument("--overlay", help="write a 3x PNG with red boxes on areas over the color limit and orange on "
                                      "areas that got no palette")
    ap.add_argument("--export", help="Mega Drive background only: indexed PNG for SGDK rescomp")
    ap.add_argument("--nes-palette", help="NES only: emulator .pal file to snap colors to (default: take colors "
                                          "as given, since the NES has no single RGB palette)")
    args = ap.parse_args(argv)
    sprite = None
    if args.sprite:
        try:
            sprite = tuple(int(v) for v in args.sprite.lower().split("x"))
            assert len(sprite) == 2
        except (ValueError, AssertionError):
            print("--sprite must look like 32x32", file=sys.stderr)
            return 2
    try:
        nes_pal = read_pal(args.nes_palette) if args.nes_palette else None
        report, con, pals = check(args.console, args.image, sprite, nes_pal)
    except (OSError, ValueError) as e:
        print(f"cannot read {args.image}: {e}", file=sys.stderr)
        return 2
    print(text_report(report))
    if args.json:
        with open(args.json, "w") as f:
            json.dump(report, f, indent=1)
    if args.overlay:
        overlay(args.image, report, args.overlay)
    if args.export:
        if args.console != "md" or sprite:
            print("--export only supports Mega Drive backgrounds", file=sys.stderr)
            return 2
        if not report["ok"]:
            print("not exported: fix the errors first", file=sys.stderr)
            return 1
        export_md(con, pals, args.export)
    return 0 if report["ok"] else 1


if __name__ == "__main__":
    sys.exit(main())
