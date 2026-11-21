#!/usr/bin/env python3
"""Turns ai-test/<model>/*/final.json into the tables in results/summary.md."""
import json
import statistics
import sys
from collections import Counter, defaultdict
from pathlib import Path

HERE = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(HERE / "tools"))
import tasks  # noqa: E402

CLOCK = {"6502": 1789773, "z80": 3579545}
EXHAUSTIVE = ["popcount", "mul8", "utoa16", "isqrt16"]  # every input tested, so the measured max is the true worst case
CPUS = ["6502", "z80"]

finals = [json.loads(p.read_text()) for p in sorted((HERE / "ai-test").glob("*/*/final.json"))]
models = sorted({f["model"] for f in finals})
out = []
P = out.append

P("## Pass rates (all vectors right)\n")
P("| Model | CPU | First try | After 1 fix | After 2 fixes | Assembled first try |")
P("| --- | --- | --- | --- | --- | --- |")
for m in models:
    for c in CPUS:
        fs = [f for f in finals if f["model"] == m and f["cpu"] == c]
        n = len(fs)
        by = lambda k: sum(1 for f in fs if f["ok"] and f["fix_rounds"] <= k)
        asm = sum(1 for f in fs if f["rounds"][0]["assembled"])
        P(f"| {m} | {c} | {by(0)}/{n} | {by(1)}/{n} | {by(2)}/{n} | {asm}/{n} |")

P("\n## First try per task (both models, 3 reps each)\n")
P("| Task | 6502 first try | 6502 final | Z80 first try | Z80 final |")
P("| --- | --- | --- | --- | --- |")
for t in tasks.TASKS:
    row = [t]
    for c in CPUS:
        fs = [f for f in finals if f["task"] == t and f["cpu"] == c]
        row += [f"{sum(f['first_try'] for f in fs)}/{len(fs)}", f"{sum(f['ok'] for f in fs)}/{len(fs)}"]
    P("| " + " | ".join(row) + " |")

P("\n## Per model and task (first try / final, out of 3)\n")
for m in models:
    P(f"\n{m}\n")
    P("| Task | 6502 | Z80 |")
    P("| --- | --- | --- |")
    for t in tasks.TASKS:
        cells = []
        for c in CPUS:
            fs = [f for f in finals if f["model"] == m and f["task"] == t and f["cpu"] == c]
            cells.append(f"{sum(f['first_try'] for f in fs)} / {sum(f['ok'] for f in fs)}")
        P(f"| {t} | " + " | ".join(cells) + " |")

P("\n## What went wrong on the first try\n")
for c in CPUS:
    kinds = Counter()
    for f in finals:
        if f["cpu"] != c or f["first_try"]:
            continue
        r0 = f["rounds"][0]
        if not r0["assembled"]:
            kinds["did not assemble"] += 1
        else:
            k = max(r0["fail_kinds"].items(), key=lambda kv: kv[1])[0]
            kinds[f"{f['task']}: {k}"] += 1
    P(f"\n{c}:\n")
    for k, n in kinds.most_common():
        P(f"- {n} x {k}")

P("\n## Cycle claims vs measured worst case (exhaustive tasks, routines that passed)\n")
P("| CPU | Routines | Exact | Within 10% | Under-claimed by >10% | Over-claimed by >10% | No claim |")
P("| --- | --- | --- | --- | --- | --- | --- |")
claims = defaultdict(list)
for f in finals:
    for r in f["rounds"]:
        if r["ok"] and f["task"] in EXHAUSTIVE:
            claims[f["cpu"]].append((r["claim"], r["cycles_max"], f))
for c in CPUS:
    xs = claims[c]
    ex = sum(1 for cl, mx, _ in xs if cl == mx)
    w10 = sum(1 for cl, mx, _ in xs if cl and abs(cl - mx) <= 0.1 * mx)
    under = sum(1 for cl, mx, _ in xs if cl and cl < 0.9 * mx)
    over = sum(1 for cl, mx, _ in xs if cl and cl > 1.1 * mx)
    none = sum(1 for cl, _, _ in xs if not cl)
    P(f"| {c} | {len(xs)} | {ex} | {w10} | {under} | {over} | {none} |")
P("\nEach claim against the measurement (task, model, rep, round: claimed / measured max):\n")
for c in CPUS:
    for cl, mx, f in sorted(claims[c], key=lambda x: (x[2]["task"], x[2]["model"], x[2]["rep"])):
        P(f"- {c} {f['task']} {f['model']} r{f['rep']}: {cl} / {mx}")

P("\n## Speed of the routines that passed (worst case over the test vectors, call and return included)\n")
P("| Task | 6502 cycles (min to max over routines) | 6502 us | Z80 cycles | Z80 us |")
P("| --- | --- | --- | --- | --- |")
for t in tasks.TASKS:
    row = [t]
    for c in CPUS:
        mx = [r["cycles_max"] for f in finals if f["task"] == t and f["cpu"] == c for r in f["rounds"] if r["ok"]]
        if mx:
            row += [f"{min(mx):,} to {max(mx):,}", f"{min(mx) / CLOCK[c] * 1e6:.0f} to {max(mx) / CLOCK[c] * 1e6:.0f}"]
        else:
            row += ["none passed", ""]
    P("| " + " | ".join(row) + " |")

P("\n## Cost and time\n")
for m in models:
    rs = [r for f in finals if f["model"] == m for r in f["rounds"]]
    cost = sum(r["cost"] or 0 for r in rs)
    P(f"- {m}: {len(rs)} `claude -p` calls, USD {cost:.2f} at API prices, median {statistics.median(r['wall_s'] for r in rs):.0f} s per call, "
      f"median {statistics.median(r['out_tokens'] for r in rs):.0f} output tokens")

text = "\n".join(out) + "\n"
(HERE / "results/summary.md").write_text(text)
print(text)
