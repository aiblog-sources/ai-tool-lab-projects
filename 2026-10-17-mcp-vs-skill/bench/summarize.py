#!/usr/bin/env python3
"""Tables for the README and the post, from the raw runs.
Usage: python3 bench/summarize.py results/rep1.jsonl results/rep2.jsonl > results/summary.md"""
import json, statistics, sys
from collections import defaultdict
from pathlib import Path

here = Path(__file__).resolve().parent
overrides = {(o["file"], o["qid"], o["phrasing"], o["condition"]): o["correct"]
             for o in json.loads((here / "overrides.json").read_text())}
runs = []
for f in sys.argv[1:]:
    for line in open(f):
        r = json.loads(line)
        r["correct"] = overrides.get((Path(f).name, r["qid"], r["phrasing"], r["condition"]), r["correct"])
        r["loaded_mcp"] = any(t["name"] == "ToolSearch" and "mcp__" in json.dumps(t["input"]) for t in r["tools"])
        r["invoked_skill"] = any(t["name"] == "Skill" for t in r["tools"])
        # The side that ran the first search: an MCP tool call, or the Skill's script through Bash.
        searches = ["mcp" if t["name"].startswith("mcp__") else "skill" for t in r["tools"]
                    if t["name"].startswith("mcp__") or (t["name"] == "Bash" and "search.mjs" in t["input"].get("command", ""))]
        r["searched"] = searches[0] if searches else None
        r["both_sides"] = len(set(searches)) == 2
        r["shell"] = any(t["name"] == "Bash" and "search.mjs" not in t["input"].get("command", "") for t in r["tools"])
        runs.append(r)
order = ["mcp", "skill", "both", "both+instr", "both-upfront", "both-vague"]
med = lambda xs: statistics.median(xs) if xs else float("nan")

def table(rows, title):
    print(f"\n### {title}\n")
    print("| Condition | Runs | Searched with the MCP tool | Searched with the Skill's script | No search | Searched with both | Invoked the Skill | Loaded the MCP schema | Other shell commands | Right answer |")
    print("| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |")
    by = defaultdict(list)
    for r in rows: by[r["condition"]].append(r)
    for c in order:
        rs = by[c]
        if not rs: continue
        n = len(rs)
        print(f"| {c} | {n} | {sum(r['searched'] == 'mcp' for r in rs)} | {sum(r['searched'] == 'skill' for r in rs)} | "
              f"{sum(r['searched'] is None for r in rs)} | {sum(r['both_sides'] for r in rs)} | {sum(r['invoked_skill'] for r in rs)} | "
              f"{sum(r['loaded_mcp'] for r in rs)} | {sum(r['shell'] for r in rs)} | {sum(r['correct'] for r in rs)} |")

table(runs, "All runs (12 questions x 2 phrasings x 2 repetitions per condition)")
table([r for r in runs if r["phrasing"] == "hinted"], "Hinted (\"Check my notes. ...\")")
table([r for r in runs if r["phrasing"] == "bare"], "Bare question")

print("\n### Cost of each path (runs that searched with exactly one side)\n")
print("| Path | Runs | Median tool calls | Median turns | Median input tokens | Median output tokens | Median wall time (s) | Median cost (USD, API prices) | Right answer |")
print("| --- | --- | --- | --- | --- | --- | --- | --- | --- |")
for side in ["mcp", "skill"]:
    for c in order:
        rs = [r for r in runs if r["condition"] == c and r["searched"] == side and not r["both_sides"]]
        if not rs: continue
        print(f"| {side} in {c} | {len(rs)} | {med([len(r['tools']) for r in rs])} | {med([r['turns'] for r in rs])} | "
              f"{med([r['input_tokens'] for r in rs]):.0f} | {med([r['output_tokens'] for r in rs]):.0f} | "
              f"{med([r['wall_s'] for r in rs]):.1f} | {med([r['cost_usd'] for r in rs]):.4f} | {sum(r['correct'] for r in rs)}/{len(rs)} |")

print("\n### Totals\n")
print(f"- Runs: {len(runs)}; tool errors: {sum(r['tool_errors'] for r in runs)}; exit codes: "
      f"{dict(sorted(defaultdict(int, {k: sum(r['exit'] == k for r in runs) for k in {r['exit'] for r in runs}}).items()))}")
print(f"- MCP status values: {sorted({s for r in runs for s in r['mcp_status']})}; skill listed when expected: "
      f"{sum(r['skill_listed'] for r in runs if r['condition'] != 'mcp')}/{sum(r['condition'] != 'mcp' for r in runs)}")
print(f"- Models: {sorted({r['model'] for r in runs})}")
print(f"- Sum of reported cost: USD {sum(r['cost_usd'] or 0 for r in runs):.2f}; sum of wall time: {sum(r['wall_s'] for r in runs)/60:.1f} min")
