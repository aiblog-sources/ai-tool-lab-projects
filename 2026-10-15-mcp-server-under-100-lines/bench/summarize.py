#!/usr/bin/env python3
"""Tables for the post from results/*.jsonl. Usage: python3 bench/summarize.py results/rep1.jsonl results/rep2.jsonl"""
import json, statistics, sys
from collections import defaultdict

import re
from pathlib import Path
rows = [dict(json.loads(l), file=Path(f).name) for f in sys.argv[1:] for l in open(f)]
# Grade again with the current questions.json (question 5's first pattern matched "120" echoed from the question).
expect = {q["id"]: q["expect"] for q in json.loads((Path(__file__).parent / "questions.json").read_text())}
for r in rows:
    r["correct"] = all(re.search(e, r["answer"], re.I) for e in expect[r["qid"]])
# Hand review of every answer the patterns marked right without a tool call (see overrides.json for reasons).
for o in json.loads((Path(__file__).parent / "overrides.json").read_text()):
    for r in rows:
        if (r["file"], r["qid"], r["phrasing"], r["condition"]) == (o["file"], o["qid"], o["phrasing"], o["condition"]):
            r["correct"] = o["correct"]
order = ["good", "vague", "vague+instr", "good-upfront"]
print(f"{len(rows)} runs, model {sorted({r['model'] for r in rows})}\n")
print("| Condition | Phrasing | Runs | Called the MCP tool | Right answer | Used Bash/Grep/Glob | Median turns | Median cost (USD) |")
print("| --- | --- | --- | --- | --- | --- | --- | --- |")
g = defaultdict(list)
for r in rows: g[(r["condition"], r["phrasing"])].append(r)
for c in order:
    for p in ["hinted", "bare"]:
        rs = g.get((c, p), [])
        if not rs: continue
        n = len(rs)
        print(f"| {c} | {p} | {n} | {sum(r['used_mcp'] for r in rs)} | {sum(r['correct'] for r in rs)} | "
              f"{sum(bool(r['builtin']) for r in rs)} | {statistics.median(r['turns'] or 0 for r in rs)} | "
              f"{statistics.median(r['cost_usd'] or 0 for r in rs):.3f} |")
print("\nRight answers without calling the tool:", [(r["condition"], r["phrasing"], r["qid"]) for r in rows if r["correct"] and not r["used_mcp"]])
print("Called the tool but answer wrong:", [(r["condition"], r["phrasing"], r["qid"]) for r in rows if not r["correct"] and r["used_mcp"]])
print("Server not connected:", [(r["condition"], r["phrasing"], r["qid"], r["mcp_status"]) for r in rows if r["mcp_status"] != ["connected"]])
print("Tool calls (MCP) per run, median by condition:", {c: statistics.median([sum(t["name"].startswith("mcp__") for t in r["tools"]) for r in rows if r["condition"] == c] or [0]) for c in order})
print("Total cost USD:", round(sum(r["cost_usd"] or 0 for r in rows), 2), " total wall min:", round(sum(r["wall_s"] for r in rows) / 60, 1))
print("Per question, bare phrasing, MCP used (of runs):")
for q in sorted({r["qid"] for r in rows}):
    print(" q%d" % q, {c: f"{sum(r['used_mcp'] for r in g[(c,'bare')] if r['qid']==q)}/{sum(1 for r in g[(c,'bare')] if r['qid']==q)}" for c in order})
