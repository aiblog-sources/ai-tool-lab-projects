#!/usr/bin/env python3
"""Tables for the post from the raw runs. Usage: python3 bench/summarize.py results/rep1.jsonl results/rep2.jsonl"""
import json, statistics, sys
from collections import defaultdict

rows = [json.loads(l) for f in sys.argv[1:] for l in open(f)]
order = list(json.load(open(__file__.rsplit("/", 1)[0] + "/conditions.json")))
by = defaultdict(list)
for r in rows:
    by[r["condition"]].append(r)
SIDES = ("mcp", "script", "plugin-script")

def browsed(r):
    """Tried to look around the file system instead of using a search path (all such attempts were denied)."""
    return any(t["name"] in ("Read", "Grep", "Glob", "LS") or
               (t["name"] == "Bash" and "search.mjs" not in t["input"].get("command", "")) for t in r["tools"])

def line(name, rs):
    first = lambda s: sum(r["first"] == s for r in rs)
    project_skill = sum(any(s == "search-notes" for s in r["skill_calls"]) for r in rs)
    plugin_skill = sum(any(s and ":" in s for s in r["skill_calls"]) for r in rs)
    return (f"| {name} | {len(rs)} | {first('mcp')} | {first('script')} | {first('plugin-script')} | {first(None)} | "
            f"{project_skill} | {plugin_skill} | {sum(browsed(r) for r in rs)} | {sum(r['correct'] for r in rs)} | "
            f"{statistics.median(r['turns'] or 0 for r in rs)} | {statistics.median(r['wall_s'] for r in rs):.1f} | "
            f"{sum(r['cost_usd'] or 0 for r in rs):.2f} |")

HEAD = ("Runs | First search: MCP | First search: project script | First search: plugin script | No search | "
        "Project Skill invoked | Plugin Skill invoked | Tried to browse files | Right | Median turns | Median s | USD |")
print("| Condition | " + HEAD)
print("| --- " * (HEAD.count("|") + 1) + "|")
for c in order:
    if by[c]: print(line(c, by[c]))

print("\nBy phrasing (hinted = the prompt starts with \"Check my notes.\")\n")
print("| Condition | Phrasing | " + HEAD)
print("| --- " * (HEAD.count("|") + 2) + "|")
for c in order:
    for ph in ("hinted", "bare"):
        rs = [r for r in by[c] if r["phrasing"] == ph]
        if rs: print(line(f"{c} | {ph}", rs))

print("\nBy repetition (first search MCP / project script / plugin script / none)\n")
for c in order:
    parts = []
    for f in sys.argv[1:]:
        rs = [r for r in (json.loads(l) for l in open(f)) if r["condition"] == c]
        parts.append("/".join(str(sum(r["first"] == s for r in rs)) for s in (*SIDES, None)))
    print(f"- {c}: " + "  ".join(parts))

print("\nTool errors (runs with at least one, and the first error text of each kind)\n")
for c in order:
    rs = [r for r in by[c] if r["tool_errors"]]
    kinds = sorted(set(e.split("\n")[0][:90] for r in rs for e in r["error_texts"]))
    print(f"- {c}: {len(rs)} runs; " + "; ".join(kinds))

print("\nWrong answers\n")
for r in rows:
    if not r["correct"]:
        print(f"- {r['condition']} q{r['qid']} {r['phrasing']} first={r['first']}: {r['answer'][:220]!r}")
print(f"\nRuns: {len(rows)}, total USD {sum(r['cost_usd'] or 0 for r in rows):.2f}, "
      f"models {sorted(set(r['model'] for r in rows if r['model']))}, "
      f"MCP servers {sorted(set(s for r in rows for s in r['mcp_servers']))}, "
      f"MCP status {sorted(set(s for r in rows for s in r['mcp_status']))}")
