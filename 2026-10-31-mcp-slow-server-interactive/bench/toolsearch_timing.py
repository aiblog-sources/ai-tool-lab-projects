#!/usr/bin/env python3
"""How long each ToolSearch call took, and whether the notes server was connected when it returned.
Reads the local session transcripts named in run.py rows. Usage: python3 bench/toolsearch_timing.py rows.jsonl"""
import json, sys
sys.path.insert(0, __import__("os").path.dirname(__file__))
from run import transcript, ms

for line in open(sys.argv[1]):
    r = json.loads(line)
    msgs = transcript(r["session"])
    t0 = None
    uses, res = {}, {}
    for m in msgs:
        if m.get("type") == "assistant":
            for b in m["message"]["content"]:
                if b["type"] == "tool_use" and b["name"] == "ToolSearch": uses[b["id"]] = (ms(m["timestamp"]), b["input"].get("query"))
        if m.get("type") == "user" and isinstance(m["message"].get("content"), list):
            for b in m["message"]["content"]:
                if b.get("type") == "tool_result" and b["tool_use_id"] in uses:
                    found = [x.get("tool_name") for x in b["content"]] if isinstance(b.get("content"), list) else str(b.get("content"))[:60]
                    res[b["tool_use_id"]] = (ms(m["timestamp"]), found)
    first_prompt = next(ms(m["timestamp"]) for m in msgs if m.get("type") == "user" and isinstance(m["message"].get("content"), str))
    conn = next((e["ms"] for e in r["server_events"] if e["event"] == "connected"), None)
    t_launch = first_prompt - r["turns"][0]["prompt_ms"]
    out = []
    for k, (tu, q) in uses.items():
        tr, found = res.get(k, (None, None))
        out.append(f"{(tu - t_launch) / 1000:.1f}s +{(tr - tu) if tr else '?'}ms q={q!r} notes={any('notes' in str(f) for f in (found or []))}")
    print(r["rep"], r["delay_ms"], r["condition"], "connected", conn and conn / 1000, "|", " ; ".join(out))
