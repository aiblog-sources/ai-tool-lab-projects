#!/usr/bin/env python3
"""Ask Claude Code (claude -p) each question under each tool-naming condition and record what it did.

Usage: python3 bench/run.py <corpus-folder> <work-folder> <out.jsonl> [model]
The work folder should be empty: it is Claude Code's cwd, so built-in file tools find nothing there.
"""
import json, os, re, subprocess, sys, time
from pathlib import Path

# No CLAUDE.md files and no auto-memory: only the prompt, the built-in tools and this MCP server.
CLEAN = {"CLAUDE_CODE_DISABLE_CLAUDE_MDS": "1", "CLAUDE_CODE_DISABLE_AUTO_MEMORY": "1"}

here = Path(__file__).resolve().parent
corpus, work, out = (Path(a).resolve() for a in sys.argv[1:4])
model = sys.argv[4] if len(sys.argv) > 4 else "sonnet"
questions = json.loads((here / "questions.json").read_text())
conditions = json.loads((here / "conditions.json").read_text())
only = os.environ.get("CONDITIONS")
if only:
    conditions = {k: v for k, v in conditions.items() if k in only.split(",")}
phrasings = {"hinted": "Check my notes. {q}", "bare": "{q}"}
done = set()
if out.exists():
    for line in out.read_text().splitlines():
        r = json.loads(line); done.add((r["qid"], r["phrasing"], r["condition"]))

def run(qid, q, phrasing, cname, c):
    cfg = work.parent / f"mcp-{cname}.json"
    cfg.write_text(json.dumps({"mcpServers": {c["server"]: {"command": "node",
        "args": [str(here.parent / "server.mjs"), str(corpus)], "env": c["env"]}}}))
    prompt = phrasings[phrasing].format(q=q["q"]) + " Answer in one or two sentences."
    cmd = ["claude", "-p", prompt, "--model", model, "--mcp-config", str(cfg), "--strict-mcp-config",
           "--setting-sources", "project", "--disallowedTools", "WebSearch", "WebFetch",
           "--allowedTools", f"mcp__{c['server']}", "--output-format", "stream-json", "--verbose"]
    env = {**os.environ, **CLEAN, **c.get("claude_env", {})}
    t0 = time.time()
    p = subprocess.run(cmd, cwd=work, env=env, capture_output=True, text=True, stdin=subprocess.DEVNULL, timeout=300)
    wall = time.time() - t0
    tools, text, result, init = [], "", {}, {}
    for line in p.stdout.splitlines():
        try: m = json.loads(line)
        except json.JSONDecodeError: continue
        if m.get("type") == "system" and m.get("subtype") == "init": init = m
        if m.get("type") == "assistant":
            for b in m["message"]["content"]:
                if b["type"] == "tool_use": tools.append({"name": b["name"], "input": b["input"]})
                if b["type"] == "text": text = b["text"]
        if m.get("type") == "result": result = m
    answer = result.get("result", text) or ""
    u = result.get("usage", {})
    return {"qid": qid, "phrasing": phrasing, "condition": cname, "model": init.get("model"),
            "mcp_status": [s.get("status") for s in init.get("mcp_servers", [])],
            "tools": tools, "used_mcp": any(t["name"].startswith("mcp__") for t in tools),
            "used_toolsearch": any(t["name"] == "ToolSearch" for t in tools),
            "builtin": [t["name"] for t in tools if not t["name"].startswith("mcp__") and t["name"] != "ToolSearch"],
            "answer": answer, "correct": all(re.search(e, answer, re.I) for e in q["expect"]),
            "turns": result.get("num_turns"), "cost_usd": result.get("total_cost_usd"),
            "duration_ms": result.get("duration_ms"), "wall_s": round(wall, 1),
            "input_tokens": u.get("input_tokens", 0) + u.get("cache_read_input_tokens", 0) + u.get("cache_creation_input_tokens", 0),
            "output_tokens": u.get("output_tokens"), "exit": p.returncode}

with out.open("a") as f:
    for q in questions:              # conditions interleaved per question, so load and drift hit them alike
        for phrasing in phrasings:
            for cname, c in conditions.items():
                if (q["id"], phrasing, cname) in done: continue
                for attempt in range(3):   # a Claude Code auto-update mid-run made launches exit at once with code 2
                    r = run(q["id"], q, phrasing, cname, c)
                    if r["model"]: break
                    time.sleep(30)
                f.write(json.dumps(r) + "\n"); f.flush()
                print(f"q{q['id']:>2} {phrasing:6} {cname:12} mcp={r['used_mcp']!s:5} ok={r['correct']!s:5} "
                      f"builtin={r['builtin']} ${r['cost_usd']} {r['wall_s']}s", flush=True)
