#!/usr/bin/env python3
"""How far each fix moved the picture: mean absolute RGB difference from v0 (0-255, transparent drawn as grey)
and the color count, for every job whose v0 failed. Writes results/fix-drift.md. MIT license."""
import json
from pathlib import Path

import numpy as np
from PIL import Image

HERE = Path(__file__).resolve().parent.parent


def rgb(p):
    im = Image.open(p).convert("RGBA")
    bg = Image.new("RGBA", im.size, (128, 128, 128, 255))
    return np.array(Image.alpha_composite(bg, im).convert("RGB")).astype(float)


runs = json.loads((HERE / "results" / "ai-test.json").read_text())
rows = ["| Job | v0 colors | Stage | Passes | Colors | Mean RGB change vs v0 |", "| --- | --- | --- | --- | --- | --- |"]
agg = {"plain1": [], "check": []}
for r in sorted(runs, key=lambda r: r["tag"]):
    v0 = r["v0"].get("recheck")
    if not v0 or v0["ok"]:
        continue
    a = rgb(HERE / "ai-test" / r["tag"] / "v0" / "art.png")
    final_check = None
    for st in ("plain1", "check1", "check2"):
        p = HERE / "ai-test" / r["tag"] / st / "art.png"
        if st not in r or not p.exists():
            continue
        b = rgb(p)
        d = float(np.abs(a - b).mean()) if a.shape == b.shape else None
        rc = r[st]["recheck"]
        rows.append(f"| {r['tag']} | {v0['info']['console_colors']} | {st} | {'yes' if rc['ok'] else 'no'} | "
                    f"{rc['info']['console_colors']} | {'size changed' if d is None else f'{d:.1f}'} |")
        if st == "plain1" and rc["ok"] and d is not None:
            agg["plain1"].append(d)
        if st != "plain1" and rc["ok"] and d is not None:
            final_check = d
    if final_check is not None:
        agg["check"].append(final_check)
summary = [f"Passing fixes, mean RGB change vs v0: plain {np.mean(agg['plain1']):.1f} (n={len(agg['plain1'])}, "
           f"median {np.median(agg['plain1']):.1f}); pixcheck {np.mean(agg['check']):.1f} (n={len(agg['check'])}, "
           f"median {np.median(agg['check']):.1f})", ""]
(HERE / "results" / "fix-drift.md").write_text("\n".join(["# Fix drift", ""] + summary + rows) + "\n")
print("\n".join(summary + rows))
