#!/usr/bin/env python3
"""Every ToolSearch call made while the notes server was not connected yet: how long it took and what it returned.
Needs the local session transcripts. Usage: python3 bench/toolsearch_stats.py rows.jsonl... > results/toolsearch.md"""
import json, os, statistics, sys
sys.path.insert(0, os.path.dirname(__file__))
from run import transcript, ms

calls, tokens, sessions = [], {"in": 0, "out": 0}, 0
for f in sys.argv[1:]:
    for line in open(f):
        r = json.loads(line)
        if r.get("mode") == "headless" or not r.get("turns"): continue
        msgs = transcript(r["session"]); sessions += 1
        t_launch = next(ms(m["timestamp"]) for m in msgs if m.get("type") == "user" and isinstance(m["message"].get("content"), str)) - r["turns"][0]["prompt_ms"]
        conn = next((e["ms"] for e in r["server_events"] if e["event"] == "connected"), None)
        uses, seen = {}, set()
        for m in msgs:
            if m.get("type") == "assistant":
                u = m["message"].get("usage", {})
                if m["message"].get("id") not in seen:   # one API reply is split over several transcript lines
                    seen.add(m["message"].get("id"))
                    tokens["in"] += u.get("input_tokens", 0) + u.get("cache_read_input_tokens", 0) + u.get("cache_creation_input_tokens", 0)
                    tokens["out"] += u.get("output_tokens", 0)
                for b in m["message"]["content"]:
                    if b["type"] == "tool_use" and b["name"] == "ToolSearch": uses[b["id"]] = (ms(m["timestamp"]) - t_launch, b["input"]["query"])
            if m.get("type") == "user" and isinstance(m["message"].get("content"), list):
                for b in m["message"]["content"]:
                    if b.get("type") == "tool_result" and b.get("tool_use_id") in uses:
                        t, q = uses[b["tool_use_id"]]
                        c = b.get("content")
                        n = len(c) if isinstance(c, list) else 0
                        if conn is None or t < conn:
                            calls.append({"select": q.startswith("select:"), "matches": n, "ms": ms(m["timestamp"]) - t_launch - t,
                                          "notes_after": conn is not None and ms(m["timestamp"]) - t_launch >= conn,
                                          "says_connecting": "still connecting" in str(c)})

def line(label, xs):
    if not xs: return
    d = sorted(x["ms"] for x in xs)
    print(f"| {label} | {len(xs)} | {statistics.median(d)} | {d[0]} to {d[-1]} | {sum(x['says_connecting'] for x in xs)} | {sum(x['notes_after'] for x in xs)} |")

print(f"ToolSearch calls made before the notes server connected ({sessions} interactive sessions).\n")
print("| Call | Calls | Median ms | Range ms | Result says 'still connecting' | Returned after the server connected |")
print("| --- | ---: | ---: | --- | ---: | ---: |")
line("Keyword search that matched other tools", [c for c in calls if not c["select"] and c["matches"]])
line("Keyword search with no match", [c for c in calls if not c["select"] and not c["matches"]])
line("`select:` of tools that did not exist yet", [c for c in calls if c["select"]])
print(f"\nTokens over all interactive sessions: {tokens['in']:,} input (incl. cache), {tokens['out']:,} output.")
