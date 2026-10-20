#!/usr/bin/env python3
"""Calls to a deferred MCP tool before ToolSearch loaded it, read from Claude Code's session transcripts
(they hold the tool results; the stream-json records of rep1/rep2 did not keep them).
Usage: SINCE=<ISO time of the first run> python3 bench/early_calls.py <transcript-dir> <runs.jsonl>...
Runs without a session_id are matched to transcripts in launch order by their prompt text.
Prints counts only: the transcripts also hold Claude Code's own system prompt and are not published."""
import json, glob, os, sys
from collections import Counter, defaultdict

tdir, files = sys.argv[1], sys.argv[2:]
sessions = []
for f in glob.glob(os.path.join(tdir, "*.jsonl")):
    ev = [json.loads(l) for l in open(f)]
    prompt = next((e["message"]["content"] for e in ev if e.get("type") == "user"
                   and isinstance(e.get("message", {}).get("content"), str)), None)
    t0 = min(e["timestamp"] for e in ev if e.get("timestamp"))
    order, seen = [], set()   # a transcript can log the same message twice; keep each tool_use/result once
    for e in ev:
        c = e.get("message", {}).get("content") if isinstance(e.get("message"), dict) else None
        for b in c if isinstance(c, list) else []:
            key = (b.get("type"), b.get("id") or b.get("tool_use_id"))
            if b.get("type") in ("tool_use", "tool_result") and key not in seen: seen.add(key); order.append(b)
    sessions.append({"id": os.path.basename(f)[:-6], "t0": t0, "prompt": prompt, "order": order})
sessions = sorted((s for s in sessions if s["t0"] >= os.environ.get("SINCE", "")), key=lambda s: s["t0"])
by_id = {s["id"]: s for s in sessions}

PH = {"hinted": "Check my notes. {q} Answer in one or two sentences.", "bare": "{q} Answer in one or two sentences."}
qs = {q["id"]: q["q"] for q in json.load(open(os.path.join(os.path.dirname(__file__), "questions.json")))}
deferred = {"good": {"search_notes", "read_note"}, "vague": {"query", "get"},
            "good+tool": {"read_note"}, "vague+tool": {"get"}}
stats, skipped, i = defaultdict(Counter), 0, 0
for f in files:   # pass run files in launch order
    rows = [json.loads(l) for l in open(f)]
    for r in rows:
        s = by_id.get(r.get("session_id"))
        if s is None:   # walk forward to the next transcript with this prompt and the same tool calls
            want, calls = PH[r["phrasing"]].format(q=qs[r["qid"]]), [t["name"] for t in r["tools"]]
            names = lambda s: [b["name"] for b in s["order"] if b.get("type") == "tool_use"]
            while (sessions[i]["prompt"], names(sessions[i])) != (want, calls):
                skipped += 1; i += 1
            s = sessions[i]; i += 1
        if r["condition"] not in deferred: continue
        loaded, uses, early = set(), {}, False
        for b in s["order"]:
            if b.get("type") == "tool_use":
                uses[b["id"]] = b
            else:
                u = uses.get(b["tool_use_id"])
                if u and u["name"] == "ToolSearch" and isinstance(b.get("content"), list):
                    loaded |= {x["tool_name"].split("__")[-1] for x in b["content"] if x.get("type") == "tool_reference"}
                elif u and u["name"].startswith("mcp__"):
                    short = u["name"].split("__")[-1]
                    if short in deferred[r["condition"]] and short not in loaded:
                        k = "error" if b.get("is_error") else "ok"
                        early = True
                        stats[r["condition"]][f"early call ({k})"] += 1
                        if b.get("is_error") or not u["input"]:
                            print("  example:", r["condition"], r["qid"], json.dumps(u["input"])[:80], "->",
                                  str(b.get("content"))[:160].replace("\n", " "))
        stats[r["condition"]]["runs"] += 1
        stats[r["condition"]]["runs with an early call"] += early
for c, v in stats.items(): print(c, dict(v))
print("transcripts skipped while matching:", skipped)
