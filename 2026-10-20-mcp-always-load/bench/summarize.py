#!/usr/bin/env python3
"""Tables for the post. Usage: python3 bench/summarize.py results/rep1.jsonl results/rep2.jsonl [results/haiku.jsonl]
Also reads results/overhead.json and results/slow.json when present."""
import json, re, statistics, sys
from collections import defaultdict
from pathlib import Path

here = Path(__file__).parent
rows = [dict(json.loads(l), file=Path(f).name) for f in sys.argv[1:] for l in open(f)]
expect = {q["id"]: q["expect"] for q in json.loads((here / "questions.json").read_text())}
for r in rows:
    r["correct"] = all(re.search(e, r["answer"], re.I) for e in expect[r["qid"]])
# Hand review of answers the patterns got wrong (see overrides.json for reasons).
for o in json.loads((here / "overrides.json").read_text()):
    for r in rows:
        if (r["file"], r["qid"], r["phrasing"], r["condition"]) == (o["file"], o["qid"], o["phrasing"], o["condition"]):
            r["correct"] = o["correct"]
order = ["good", "good+tool", "good+server", "good+off", "vague", "vague+tool", "vague+off"]
med = lambda xs: statistics.median(xs) if xs else 0
for model in sorted({r["model"] for r in rows}):
    rs_m = [r for r in rows if r["model"] == model]
    print(f"\n## {model}: {len(rs_m)} runs\n")
    print("| Condition | Phrasing | Runs | Called the MCP tool | ToolSearch before it | Right answer | Median first-request input tokens | Median wall s | Median cost (USD) |")
    print("| --- | --- | --- | --- | --- | --- | --- | --- | --- |")
    g = defaultdict(list)
    for r in rs_m: g[(r["condition"], r["phrasing"])].append(r)
    for c in order:
        for p in ["hinted", "bare", "both"]:
            rs = [r for r in rs_m if r["condition"] == c and (p == "both" or r["phrasing"] == p)]
            if not rs: continue
            print(f"| {c} | {p} | {len(rs)} | {sum(r['used_mcp'] for r in rs)} | {sum(r['searched_first'] for r in rs)} | "
                  f"{sum(r['correct'] for r in rs)} | {med([r['first_input_tokens'] or 0 for r in rs]):.0f} | "
                  f"{med([r['wall_s'] for r in rs]):.1f} | {med([r['cost_usd'] or 0 for r in rs]):.4f} |")
    print("\nRight without the tool:", [(r["file"], r["condition"], r["phrasing"], r["qid"]) for r in rs_m if r["correct"] and not r["used_mcp"]])
    print("Tool called, answer wrong:", [(r["file"], r["condition"], r["phrasing"], r["qid"], r["answer"][:90]) for r in rs_m if not r["correct"] and r["used_mcp"]])
    print("Server not connected:", [(r["condition"], r["qid"], r["mcp_status"]) for r in rs_m if r["mcp_status"] != ["connected"]])
    print("Built-in tools used:", sorted({n for r in rs_m for n in r["builtin"]}))
    print("Turns, median by condition:", {c: med([r["turns"] or 0 for r in rs_m if r["condition"] == c]) for c in order})
    print("Total cost USD:", round(sum(r["cost_usd"] or 0 for r in rs_m), 2), " wall min:", round(sum(r["wall_s"] for r in rs_m) / 60, 1))
    print("Bare phrasing, MCP used per question:")
    for q in sorted({r["qid"] for r in rs_m}):
        print(" q%d" % q, {c: f"{sum(r['used_mcp'] for r in g[(c, 'bare')] if r['qid'] == q)}/{sum(1 for r in g[(c, 'bare')] if r['qid'] == q)}" for c in order if g[(c, 'bare')]})

res = Path(sys.argv[1]).parent
if (res / "overhead.json").exists():
    o = json.loads((res / "overhead.json").read_text())
    print("\n## Context of a one-turn prompt (input tokens, 3 launches each)\n")
    base = med([r["first_input_tokens"] for r in o if r["condition"] == "none"])
    for c in dict.fromkeys(r["condition"] for r in o):
        v = [r["first_input_tokens"] for r in o if r["condition"] == c]
        print(f"| {c} | {sorted(set(v))} | {med(v) - base:+.0f} vs none |")
if (res / "slow.json").exists():
    s = json.loads((res / "slow.json").read_text())
    print("\n## Slow server (STARTUP_DELAY_MS), 2 launches each\n")
    for k in dict.fromkeys((r["delay_ms"], r["stream_input"], r["condition"]) for r in s):
        v = [r for r in s if (r["delay_ms"], r["stream_input"], r["condition"]) == k]
        print(k, "status", [x["mcp_status"] for x in v], "first tokens", [x["first_input_tokens"] for x in v],
              "tools", [[t["name"].split("__")[-1] for t in x["tools"]] for x in v], "wall", [x["wall_s"] for x in v],
              "right", [bool(re.search(r"7\.1", x["answer"])) for x in v])
