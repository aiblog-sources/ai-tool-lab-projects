"""One table from ai-test/*/v*-frames.csv (MIT). Lag is summed from per-frame
deltas of the one-byte LAG counter, so it does not wrap at 256."""
import csv, json, re, sys
from pathlib import Path

FROM = 60
root = Path(__file__).resolve().parent.parent / "ai-test"
out = []
for d in sorted(p for p in root.iterdir() if p.is_dir()):
    res = json.loads((d / "result.json").read_text())
    for stage in ("v0", "v1"):
        f = d / f"{stage}-frames.csv"
        if not f.exists():
            continue
        rows = list(csv.DictReader(open(f)))
        lag = sum((int(b["lag"]) - int(a["lag"])) % 256 for a, b in zip(rows, rows[1:]) if int(b["frame"]) >= FROM)
        rows = [r for r in rows if int(r["frame"]) >= FROM]
        hit = [r for r in rows if int(r["sprites_lost"])]
        ex = (d / f"{stage}-explain.txt").read_text()
        m = re.search(r"stayed invisible: (\d+)", ex)
        out.append(dict(id=d.name, stage=stage, frames=len(rows), pct_lost=round(100 * len(hit) / len(rows), 1),
                        max_lost=max([int(r["sprites_lost"]) for r in rows] + [0]),
                        max_on_line=max(int(r["max_on_line"]) for r in rows),
                        longest_invisible=int(m.group(1)) if m else 0, lag=lag,
                        real_lines=sum(int(r["real_lines"]) for r in rows), tp=sum(int(r["tp"]) for r in rows),
                        fp=sum(int(r["fp"]) for r in rows), fn=sum(int(r["fn"]) for r in rows),
                        flag_wrong=sum(1 for r in rows if r["flag_hw"] != r["flag_exact"]),
                        build_rounds=res[stage].get("build_rounds"), cost=res["cost_usd"] if stage == "v0" else ""))
json.dump(out, open(root / "table.json", "w"), indent=1)
keys = list(out[0])
print("| " + " | ".join(keys) + " |")
for r in out:
    print("| " + " | ".join(str(r[k]) for k in keys) + " |")
print("total USD", round(sum(json.loads((d / 'result.json').read_text())['cost_usd'] for d in root.iterdir() if d.is_dir()), 2))
