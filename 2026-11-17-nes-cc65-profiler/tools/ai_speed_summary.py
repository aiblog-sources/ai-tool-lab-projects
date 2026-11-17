#!/usr/bin/env python3
"""Table of the ai_speed.py runs (MIT). Re-checks the picture from frame 0
(the first pass compared from frame 60, which lines up different game loops)
and writes ai-speed/summary.json and results/ai-speed-table.md."""
import json
from pathlib import Path

import same_picture

HERE = Path(__file__).resolve().parent.parent
ORDER = ["plain", "profile", "lines", "spikes", "loop-plain", "loop-spikes"]
rows = []
for f in sorted((HERE / "ai-speed").glob("*/result.json")):
    r = json.loads(f.read_text())
    s = HERE / ".scratch" / "ai-speed" / f.parent.name
    if r.get("builds"):
        r["picture"] = same_picture.compare(s / "orig-oam.csv", s / "fix-oam.csv")
        f.write_text(json.dumps(r, indent=1))
    rows.append(r)
rows.sort(key=lambda r: (r["scene"], ORDER.index(r["condition"]), r["rep"]))
(HERE / "ai-speed" / "summary.json").write_text(json.dumps(rows, indent=1) + "\n")
out = ["| scene | prompt | rep | slow loops | busy/loop | feedback rounds | same picture | USD |",
       "| --- | --- | --- | --- | --- | --- | --- | --- |"]
for r in rows:
    a = r.get("after", {})
    pic = r.get("picture", {})
    same = "yes" if pic.get("same") else f"no (from table {pic.get('first_diff')})" if pic else "-"
    out.append(f"| {r['scene']} | {r['condition']} | {r['rep']} | {r['before']['slow_loops']} to {a.get('slow_loops', '-')} | "
               f"{r['before']['busy_per_loop_avg']:,} to {a.get('busy_per_loop_avg', 0):,} | "
               f"{max(0, len(r.get('rounds', [])) - 1) if r['condition'].startswith('loop-') else '-'} | {same} | {r['cost_usd']:.3f} |")
(HERE / "results" / "ai-speed-table.md").write_text("\n".join(out) + "\n")
print("\n".join(out))
print("total USD", round(sum(r["cost_usd"] for r in rows), 3))
