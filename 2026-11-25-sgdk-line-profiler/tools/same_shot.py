#!/usr/bin/env python3
"""Rough "does it still look the same" check: frame 600 of each fixed version vs the original (v0), same pad
script. Prints, per model and stage, how many programs show an identical frame and the median share of pixels
that differ. Random number use or timing changes can move things legitimately, so this is a hint, not a verdict."""
import json
import statistics as st
import sys
from collections import defaultdict
from pathlib import Path

C = Path(__file__).resolve().parent.parent / "results" / "corpus"


def ppm(p):
    b = p.read_bytes()
    parts, i = [], 0
    while len(parts) < 4:  # P6 w h max
        j = b.index(b"\n", i) if b[i:i + 1] != b"#" else b.index(b"\n", i)
        tok = b[i:j].split()
        if tok and not tok[0].startswith(b"#"):
            parts += tok
        i = j + 1
    return int(parts[1]), int(parts[2]), b[i:]


res = defaultdict(list)
for d in sorted(C.iterdir()):
    v0 = d / "v0-shot.ppm"
    if not v0.exists():
        continue
    model = json.loads((d / "v0.json").read_text())["tag"].rsplit("-r", 1)[0].split("-", 1)[1]
    w, h, a = ppm(v0)
    for stage in ("lint2", "plain", "hot"):
        s = d / f"{stage}-shot.ppm"
        if not s.exists():
            continue
        w2, h2, b = ppm(s)
        if (w, h) != (w2, h2):
            res[(model, stage)].append(1.0)
            continue
        n = w * h
        diff = sum(1 for k in range(n) if a[3 * k:3 * k + 3] != b[3 * k:3 * k + 3])
        res[(model, stage)].append(diff / n)
for (m, s), v in sorted(res.items()):
    print(f"{m:12} {s:6} programs {len(v):2}  identical {sum(x == 0 for x in v):2}  "
          f"median pixels differing {100 * st.median(v):.2f}%  over 5%: {sum(x > 0.05 for x in v)}")
