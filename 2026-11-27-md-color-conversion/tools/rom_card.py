#!/usr/bin/env python3
"""Build the ramp test card into a Mega Drive ROM four ways and check what Genesis Plus GX shows.

  snes      the card with its SNES colors, straight into rescomp (rescomp truncates)
  round/lab/distinct   the card converted by mdcolor first

For each, reads the emulator screenshot at the center of every swatch, turns the RGB back into 3-bit levels with
the table measured by probe_rescomp.py, and compares with mdcolor's prediction. Writes results/rom-card.json and
the screenshots to results/shots/. Needs Docker, the SGDK image and mdprof (see probe_rescomp.py). MIT license."""
import csv, json, os, shutil, subprocess, sys
from pathlib import Path

import numpy as np
from PIL import Image

HERE = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(HERE))
sys.path.insert(0, str(HERE / "tools"))
import mdcolor as m  # noqa: E402
import testcard  # noqa: E402
from probe_rescomp import build_and_shoot, read_palette_words  # noqa: E402


def emu_tables():
    rows = [r for r in csv.DictReader(open(HERE / "results/rescomp-probe.csv")) if int(r["v"]) % 16]
    t = {}
    for ch in "rgb":
        t[ch] = {int(r[f"emu_{ch}"]): int(r[f"{ch}_level"]) for r in rows}
    return t


def hue_shift(lab1, lab2):
    """Hue angle change in degrees per color, 0 where either color is near grey (chroma under 8)."""
    h1 = np.degrees(np.arctan2(lab1[:, 2], lab1[:, 1]))
    h2 = np.degrees(np.arctan2(lab2[:, 2], lab2[:, 1]))
    d = np.abs((h2 - h1 + 180) % 360 - 180)
    grey = (np.hypot(lab1[:, 1], lab1[:, 2]) < 8) | (np.hypot(lab2[:, 1], lab2[:, 2]) < 8)
    return np.where(grey, 0, d)


def main():
    tables = emu_tables()
    snes = testcard.snes_palettes()
    out = {}
    (HERE / "results/shots").mkdir(parents=True, exist_ok=True)
    for method in ["snes", "round", "lab", "distinct"]:
        if method == "snes":
            pals = snes
            predicted = [m.convert_colors(np.array(p), "trunc") for p in snes]
        else:
            predicted = [m.convert_colors(np.array(p), method) for p in snes]
            pals = [[tuple(int(v) for v in m.levels_to_rgb(l, "linear")) for l in lv] for lv in predicted]
        img = testcard.card(pals)
        d = build_and_shoot(lambda path: img.save(path), f"card-{method}")
        words = read_palette_words(d)
        shot = np.array(Image.open(d / "shot.ppm").convert("RGB"))
        Image.fromarray(shot).save(HERE / f"results/shots/card-{method}.png")
        mism, ramps = 0, {}
        for p, name in enumerate(testcard.RAMPS):
            seen = []
            for s in range(15):
                y, x = 16 + p * 48 + 20, 20 + s * 19 + 8
                rgb = shot[y, x]
                lv = tuple(tables[ch][int(v)] for ch, v in zip("rgb", rgb))
                seen.append(lv)
                w = words[p * 16 + 1 + s]
                rom_lv = ((w >> 1) & 7, (w >> 5) & 7, (w >> 9) & 7)
                mism += (lv != tuple(predicted[p][s])) + (rom_lv != tuple(predicted[p][s]))
            src_lab = m.srgb_to_lab(np.array(snes[p]))
            de = m.ciede2000(src_lab, m.srgb_to_lab(m.levels_to_rgb(np.array(seen), "linear")))
            seen_lab = m.srgb_to_lab(m.levels_to_rgb(np.array(seen), "linear"))
            src_c = np.hypot(src_lab[:, 1], src_lab[:, 2])
            out_c = np.hypot(seen_lab[:, 1], seen_lab[:, 2])
            # a ramp should get lighter step by step: count steps where L* goes down
            reversals = int((np.diff(seen_lab[:, 0]) < -0.5).sum())
            ramps[name] = dict(shades_seen=len(set(seen)), lightness_reversals=reversals,
                               black=int(sum(1 for x in seen if x == (0, 0, 0))),
                               mean_chroma_change=round(float((out_c - src_c).mean()), 2),
                               max_hue_shift_deg=round(float(hue_shift(src_lab, seen_lab).max()), 1),
                               levels=[list(map(int, x)) for x in seen],
                               mean_de=round(float(de.mean()), 2), max_de=round(float(de.max()), 2))
        out[method] = dict(mismatches_vs_prediction=mism, ramps=ramps)
        print(method, "mismatches", mism, {k: (v["shades_seen"], v["lightness_reversals"], v["black"], v["mean_de"], v["max_de"], v["mean_chroma_change"], v["max_hue_shift_deg"]) for k, v in ramps.items()})
    json.dump(out, open(HERE / "results/rom-card.json", "w"), indent=1)


if __name__ == "__main__":
    main()
