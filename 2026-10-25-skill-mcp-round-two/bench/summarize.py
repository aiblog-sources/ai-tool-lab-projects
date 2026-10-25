#!/usr/bin/env python3
"""Tables for the post from the raw runs. Usage: python3 bench/summarize.py results/rep1.jsonl results/rep2.jsonl"""
import json, statistics, sys
from collections import defaultdict

rows = [json.loads(l) for f in sys.argv[1:] for l in open(f)]
order = list(json.load(open(__file__.rsplit("/", 1)[0] + "/conditions.json")))
by = defaultdict(list)
for r in rows:
    by[r["condition"]].append(r)

def browsed(r):
    """Tried to look around the file system instead of using a search path (all such attempts were denied)."""
    return any(t["name"] in ("Read", "Grep", "Glob", "LS") or
               (t["name"] == "Bash" and "search.mjs" not in t["input"].get("command", "")) for t in r["tools"])

def line(name, rs):
    n = len(rs)
    first = lambda s: sum(r["first"] == s for r in rs)
    bridge = sum(r["skill_before_search"] and r["first"] == "mcp" for r in rs)
    return (f"| {name} | {n} | {first('mcp')} | {first('script')} | {first(None)} | {sum(r['skill_invoked'] for r in rs)} | "
            f"{bridge} | {sum(browsed(r) for r in rs)} | {sum(r['correct'] for r in rs)} | "
            f"{statistics.median(r['turns'] or 0 for r in rs)} | {statistics.median(r['wall_s'] for r in rs):.1f} | "
            f"{sum(r['cost_usd'] or 0 for r in rs):.2f} |")

print("| Condition | Runs | Searched with MCP | Searched with script | No search | Skill invoked | Skill then MCP | "
      "Tried to browse files | Right | Median turns | Median s | USD |")
print("| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |")
for c in order:
    if by[c]: print(line(c, by[c]))

print("\nBy phrasing\n")
print("| Condition | Phrasing | Runs | Searched with MCP | Searched with script | No search | Skill invoked | Skill then MCP | Tried to browse files | Right | Median turns | Median s | USD |")
print("| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |")
for c in order:
    for ph in ("hinted", "bare"):
        rs = [r for r in by[c] if r["phrasing"] == ph]
        if rs: print(line(f"{c} | {ph}", rs))

print("\nBy repetition (searched with MCP / script / none)\n")
for c in order:
    parts = []
    for f in sys.argv[1:]:
        rs = [json.loads(l) for l in open(f)]
        rs = [r for r in rs if r["condition"] == c]
        parts.append(f"{sum(r['first']=='mcp' for r in rs)}/{sum(r['first']=='script' for r in rs)}/{sum(r['first'] is None for r in rs)}")
    print(f"- {c}: " + "  ".join(parts))

print("\nWrong answers\n")
for r in rows:
    if not r["correct"]:
        print(f"- {r['condition']} q{r['qid']} {r['phrasing']} first={r['first']}: {r['answer'][:220]!r}")
print(f"\nRuns: {len(rows)}, total USD {sum(r['cost_usd'] or 0 for r in rows):.2f}, "
      f"models {sorted(set(r['model'] for r in rows))}, mcp status {sorted(set(s for r in rows for s in r['mcp_status']))}")
