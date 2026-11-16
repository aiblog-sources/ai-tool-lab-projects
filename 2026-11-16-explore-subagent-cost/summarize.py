#!/usr/bin/env python3
"""Per-condition table from runs.jsonl (+ grades.psv and grading-key.json if present).

usage: summarize.py results/runs.jsonl [results/grades.psv results/grading-key.json]
"""
import json
import statistics as st
import sys
from collections import defaultdict

runs = [json.loads(l) for l in open(sys.argv[1])]
grades = {}
if len(sys.argv) > 3:
    key = json.load(open(sys.argv[3]))
    for line in open(sys.argv[2]).read().splitlines()[1:]:
        label, pts, wrong, *_ = line.split("|")
        k = key[label]
        grades[(k["id"], k["condition"], k["rep"])] = (pts.count("1"), int(wrong))
CONDS = ["no-subagent", "default", "explore", "explore-haiku"]
by = defaultdict(list)
for r in runs:
    by[r["condition"]].append(r)
med = lambda xs: st.median(xs) if xs else 0
print("| Condition | Runs | Explore used | Cost (total) | Cost per run (median) | Haiku share | Main context at end (median) | Main input, all turns (median) | Subagent input (median) | Wall s (median) | Points | Wrong claims |")
print("|---|---|---|---|---|---|---|---|---|---|---|---|")
for c in CONDS:
    rs = by.get(c, [])
    if not rs:
        continue
    used = sum(1 for r in rs if r["agent_calls"])
    tot = sum(r["cost_usd"] for r in rs)
    haiku = sum(r["cost_by_model"].get("claude-haiku-5-5", 0) for r in rs)
    g = [grades[(r["id"], c, r["rep"])] for r in rs if (r["id"], c, r["rep"]) in grades]
    pts = f"{sum(x[0] for x in g)}/{5 * len(g)}" if g else "-"
    wrong = sum(x[1] for x in g) if g else "-"
    print(f"| {c} | {len(rs)} | {used} | ${tot:.2f} | ${med([r['cost_usd'] for r in rs]):.3f} | "
          f"{100 * haiku / tot:.0f}% | {med([r['main_ctx_final'] for r in rs]):,.0f} | "
          f"{med([r['main_input_total'] for r in rs]):,.0f} | {med([r['sub_input_total'] for r in rs]):,.0f} | "
          f"{med([r['wall_s'] for r in rs]):.0f} | {pts} | {wrong} |")
print()
print("Per repo, median cost and wall time:")
for repo in ["ky", "requests", "diff-tour"]:
    cells = []
    for c in CONDS:
        rs = [r for r in by.get(c, []) if r["repo"] == repo]
        if rs:
            cells.append(f"{c} ${med([r['cost_usd'] for r in rs]):.3f} {med([r['wall_s'] for r in rs]):.0f}s")
    print(f"- {repo}: " + "; ".join(cells))
print()
print("Agent calls by type:", dict(sorted(
    {t: sum(r["agent_calls"].count(t) for r in runs) for r in runs for t in r["agent_calls"]}.items())))
print("Subagent models:", sorted({m for r in runs for m in r["sub_models"]}))
print("Runs with 2+ result events:", {c: sum(1 for r in by[c] if r["results"] > 1) for c in CONDS if c in by})
print("Claude Code versions:", sorted({r.get("version") for r in runs}))
print("Tool calls (median main / sub):", {c: (med([r['main_tools'] for r in by[c]]), med([r['sub_tools'] for r in by[c]])) for c in CONDS if c in by})
