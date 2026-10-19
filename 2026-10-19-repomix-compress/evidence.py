#!/usr/bin/env python3
"""For each question, check whether its evidence string survives in each packed file.

usage: evidence.py PACKS_DIR > results/evidence.csv
"""
import csv, json, sys
from pathlib import Path

packs = Path(sys.argv[1])
qs = json.load(open(Path(__file__).parent / "questions.json"))["questions"]
w = csv.writer(sys.stdout)
w.writerow(["id", "repo", "full", "nocomments", "compress"])
for q in qs:
    row = [q["id"], q["repo"]]
    for mode in ["full", "nocomments", "compress"]:
        text = (packs / f"{q['repo']}-{mode}.xml").read_text()
        row.append(int(any(e in text for e in q["evidence"])))
    w.writerow(row)
