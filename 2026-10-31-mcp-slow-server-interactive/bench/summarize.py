#!/usr/bin/env python3
"""Tables for the post from run.py output. Usage: python3 bench/summarize.py results/*.jsonl"""
import json, re, sys
from collections import defaultdict

NUM = re.compile(r"\b\d+(?:\.\d+)?\s*(?:tokens?/s|tokens? per second|t/s)", re.I)

def first(r, ev):
    return next((e["ms"] for e in r["server_events"] if e["event"] == ev), None)

def classify(t):
    """What the user got on a turn: the right number, a wrong number, or no number."""
    if t["right"]: return "right"
    return "wrong number" if NUM.search(t["text"]) else "no number"

rows = [json.loads(l) for f in sys.argv[1:] for l in open(f) if "\"mode\": \"headless\"" not in l]
groups = defaultdict(list)
for r in rows:
    env = ";".join(f"{k}={v}" for k, v in r.get("claude_env", {}).items()) or "-"
    groups[(r["model"], env, r["delay_ms"], r["condition"], r.get("type_delay_s", 0))].append(r)

print("| Model | Claude Code env | Server delay | Loading | Typed after | Runs | Turn 1: search called | "
      "Turn 1: right | Turn 1 answer (no number / wrong) | Turn 2: right | Prompt sent (s) | Server connected (s) | Turn 1 first request (s) |")
print("| --- | --- | ---: | --- | ---: | ---: | ---: | ---: | --- | ---: | --- | --- | --- |")
for (model, env, d, c, td), rs in sorted(groups.items(), key=lambda x: (x[0][0] != "sonnet", x[0][0], x[0][1], x[0][2], x[0][3], x[0][4])):
    ok = [r for r in rs if r.get("turns")]
    t1 = [r["turns"][0] for r in ok]
    t2 = [r["turns"][1] for r in ok if len(r["turns"]) > 1]
    kinds = [classify(t) for t in t1 if not t["right"]]
    fmt = lambda xs: ", ".join(f"{x / 1000:.1f}" if x is not None else "not yet" for x in xs)
    print(f"| {model} | {env} | {d / 1000:g} s | {c} | {td:g} s | {len(rs)} | "
          f"{sum(t['called_search'] for t in t1)}/{len(t1)} | {sum(t['right'] for t in t1)}/{len(t1)} | "
          f"{kinds.count('no number')} / {kinds.count('wrong number')} | "
          f"{sum(t['right'] for t in t2)}/{len(t2)} | {fmt([r.get('sent_ms') for r in ok])} | "
          f"{fmt([first(r, 'connected') for r in ok])} | {fmt([t['requests'][0]['ms'] if t['requests'] else None for t in t1])} |")
    for r in rs:
        if r.get("error"): print(f"|  | error: {r['error']} |")

# headless rows (bench/headless.py) have their own shape
hl = [json.loads(l) for f in sys.argv[1:] for l in open(f) if '"mode": "headless"' in l]
if hl:
    print("\n| `claude -p` | Server delay | Loading | Runs | Search called | Right | Server connected (s) | Wall time (s) |")
    print("| --- | ---: | --- | ---: | ---: | ---: | --- | --- |")
    g = defaultdict(list)
    for r in hl: g[(r["delay_ms"], r["condition"])].append(r)
    for (d, c), rs in sorted(g.items()):
        f = lambda xs: ", ".join(f"{x / 1000:.1f}" if x is not None else "not yet" for x in xs)
        print(f"| headless | {d / 1000:g} s | {c} | {len(rs)} | {sum(r['called_search'] for r in rs)}/{len(rs)} | "
              f"{sum(r['right'] for r in rs)}/{len(rs)} | {f([r['connected_ms'] for r in rs])} | {f([r['wall_ms'] for r in rs])} |")
    print(f"\nHeadless cost: USD {sum(r['cost_usd'] or 0 for r in hl):.2f} for {len(hl)} runs (Claude Code's own figure, API prices).")

print('\n"not yet": the server had not connected when the session ended (each session ends after its last turn). '
      "The 40 s servers never connect because Claude Code's MCP_TIMEOUT (30 s by default) stops them first.")
