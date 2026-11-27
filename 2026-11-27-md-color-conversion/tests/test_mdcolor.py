#!/usr/bin/env python3
"""Checks for mdcolor: CIEDE2000 against pairs from Sharma, Wu and Dalal's published test data, sRGB->Lab on known
values, the CRAM bit layout, and rescomp's truncation as measured in results/rescomp-probe.csv. MIT license."""
import csv
import sys
from pathlib import Path

import numpy as np

HERE = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(HERE))
import mdcolor as m  # noqa: E402

SHARMA = [  # (Lab1, Lab2, expected dE00), from "The CIEDE2000 color-difference formula: implementation notes" (2005)
    ((50.0, 2.6772, -79.7751), (50.0, 0.0, -82.7485), 2.0425),
    ((50.0, 0.0, 0.0), (50.0, -1.0, 2.0), 2.3669),
    ((50.0, 2.5, 0.0), (73.0, 25.0, -18.0), 27.1492),
    ((60.2574, -34.0099, 36.2677), (60.4626, -34.1751, 39.4387), 1.2644),
    ((2.0776, 0.0795, -1.1350), (0.9033, -0.0636, -0.5514), 0.9082),
    ((50.0, 2.49, -0.001), (50.0, -2.49, 0.0009), 7.1792),
]


def main():
    fails = 0
    for a, b, want in SHARMA:
        got = float(m.ciede2000(np.array(a), np.array(b)))
        ok = abs(got - want) < 1e-3
        fails += not ok
        print(f"{'ok  ' if ok else 'FAIL'} dE00 {a} {b}: {got:.4f} (want {want})")
    lab_white = m.srgb_to_lab([255, 255, 255])
    ok = abs(lab_white[0] - 100) < 0.01 and abs(lab_white[1]) < 0.01 and abs(lab_white[2]) < 0.01
    fails += not ok
    print(f"{'ok  ' if ok else 'FAIL'} white -> Lab {np.round(lab_white, 3)}")
    ok = m.cram_word((7, 0, 0)) == 0x000E and m.cram_word((0, 7, 0)) == 0x00E0 and m.cram_word((0, 0, 7)) == 0x0E00
    fails += not ok
    print(f"{'ok  ' if ok else 'FAIL'} CRAM layout 0000BBB0GGG0RRR0")
    rows = list(csv.DictReader(open(HERE / "results/rescomp-probe.csv")))
    bad = [r for r in rows if int(r["r_level"]) != int(m.convert_colors([[int(r["v"])] * 3], "trunc")[0][0])]
    fails += bool(bad)
    print(f"{'ok  ' if not bad else 'FAIL'} trunc matches rescomp on {len(rows) - len(bad)} of {len(rows)} grey values")
    rows = list(csv.DictReader(open(HERE / "results/macro-probe.csv")))
    bad = [r for r in rows if int(r["r_level"]) != int(m.convert_colors([[int(r["v"])] * 3], "macro")[0][0])]
    fails += bool(bad)
    print(f"{'ok  ' if not bad else 'FAIL'} macro matches RGB24_TO_VDPCOLOR on {len(rows) - len(bad)} of {len(rows)} grey values")
    print("all passed" if not fails else f"{fails} failed")
    sys.exit(1 if fails else 0)


if __name__ == "__main__":
    main()
