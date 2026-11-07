"""Table of the AI test from the per-frame CSVs: boot (until the last dropped frame before frame 700) and steady
play (frames 700-1499, every scene had finished booting by then). usage: ai_summary.py [ai-test dir]"""
import csv, json, statistics, sys
from pathlib import Path

D = Path(sys.argv[1] if len(sys.argv) > 1 else Path(__file__).resolve().parent.parent / "ai-test")
STEADY = 700
print(f"{'scene':<18}{'ver':<4}{'boot s':>7}{'fps':>6}{'queued max':>11}{'median':>7}{'spill':>6}{'SAT late':>9}"
      f"{'load':>6}{'p95':>6}  USD")
total = 0
for res in sorted(D.glob("*/result.json")):
    r = json.loads(res.read_text())
    total += r["cost_usd"]
    for st in ("v0", "v1"):
        f = res.parent / f"{st}-frames.csv"
        if not f.exists():
            continue
        rows = list(csv.DictReader(open(f)))
        boot = max([int(x["frame"]) for x in rows if x["dropped"] == "1" and int(x["frame"]) < STEADY] + [0])
        s = [x for x in rows if int(x["frame"]) >= STEADY]
        q = [int(x["queued"]) for x in s if int(x["flushes"]) > 0]
        loops = sum(1 for x in s if int(x["flushes"]) > 0)
        loads = sorted(float(x["load_pct"]) for x in s)
        fps = 60 * loops / len(s)
        bt = f"{boot / 60:>7.1f}" if fps > 59.5 else f"{'-':>7}"
        print(f"{r['id']:<18}{st:<4}{bt}{60 * loops / len(s):>6.1f}{max(q + [0]):>11}"
              f"{(statistics.median(q) if q else 0):>7.0f}{sum(1 for x in s if int(x['sent_active'])):>6}"
              f"{sum(1 for x in s if int(x['sat_line']) >= 0):>9}{statistics.mean(loads):>6.1f}"
              f"{loads[int(0.95 * (len(loads) - 1))]:>6.1f}  {r['cost_usd'] if st == 'v0' else ''}")
print(f"total USD {total:.2f} (calls: {sum(json.loads(p.read_text())['calls'] for p in D.glob('*/result.json'))})")
