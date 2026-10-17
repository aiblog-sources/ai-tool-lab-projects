#!/usr/bin/env python3
"""Give Claude Code (claude -p) the same notes search as an MCP server, as a Skill, or both, and record which it uses.

Usage: python3 bench/run.py <corpus-folder> <scratch-folder> <out.jsonl> [model]
The scratch folder gets two working folders: one empty, one holding only .claude/skills/search-notes.
"""
import json, os, re, subprocess, sys, time
from pathlib import Path

# No CLAUDE.md files and no auto-memory: only the prompt, the built-in tools, the Skill and the MCP server.
CLEAN = {"CLAUDE_CODE_DISABLE_CLAUDE_MDS": "1", "CLAUDE_CODE_DISABLE_AUTO_MEMORY": "1"}

root = Path(__file__).resolve().parent.parent
corpus, scratch, out = (Path(a).resolve() for a in sys.argv[1:4])
model = sys.argv[4] if len(sys.argv) > 4 else "sonnet"
questions = json.loads((root / "bench/questions.json").read_text())
conditions = json.loads((root / "bench/conditions.json").read_text())
only = os.environ.get("CONDITIONS")
if only:
    conditions = {k: v for k, v in conditions.items() if k in only.split(",")}
qids = os.environ.get("QIDS")
if qids:
    questions = [q for q in questions if str(q["id"]) in qids.split(",")]
phrasings = {"hinted": "Check my notes. {q}", "bare": "{q}"}

work = {False: scratch / "work-plain", True: scratch / "work-skill"}
for w in work.values():
    w.mkdir(parents=True, exist_ok=True)
link = work[True] / ".claude/skills/search-notes"
link.parent.mkdir(parents=True, exist_ok=True)
if not link.exists():
    link.symlink_to(root / "skill/search-notes")

done = set()
if out.exists():
    for line in out.read_text().splitlines():
        r = json.loads(line); done.add((r["qid"], r["phrasing"], r["condition"]))

def path_of(t):
    """Which side a tool call belongs to: the MCP server, the Skill, or neither."""
    if t["name"].startswith("mcp__"): return "mcp"
    if t["name"] == "Skill": return "skill"
    if t["name"] == "Bash" and "search.mjs" in t["input"].get("command", ""): return "skill"
    if t["name"] == "Read" and "search-notes" in t["input"].get("file_path", ""): return "skill"
    return None

def run(qid, q, phrasing, cname, c):
    servers, name = {}, c.get("server", "notes")
    if c["mcp"]:
        servers[name] = {"command": "node", "args": [str(root / "server.mjs"), str(corpus)], "env": c["env"]}
    cfg = scratch / f"mcp-{cname}.json"
    cfg.write_text(json.dumps({"mcpServers": servers}))
    prompt = phrasings[phrasing].format(q=q["q"]) + " Answer in one or two sentences."
    # Same permissions in every condition: the MCP server, the Skill tool and `node` in Bash.
    cmd = ["claude", "-p", prompt, "--model", model, "--mcp-config", str(cfg), "--strict-mcp-config",
           "--setting-sources", "project", "--disallowedTools", "WebSearch", "WebFetch",
           "--allowedTools", f"mcp__{name}", "Skill", "Bash(node:*)", "--output-format", "stream-json", "--verbose"]
    env = {**os.environ, **CLEAN, "NOTES_DIR": str(corpus), **c.get("claude_env", {})}
    t0 = time.time()
    p = subprocess.run(cmd, cwd=work[c["skill"]], env=env, capture_output=True, text=True,
                       stdin=subprocess.DEVNULL, timeout=300)
    wall = time.time() - t0
    tools, text, result, init, errors = [], "", {}, {}, 0
    for line in p.stdout.splitlines():
        try: m = json.loads(line)
        except json.JSONDecodeError: continue
        if m.get("type") == "system" and m.get("subtype") == "init": init = m
        if m.get("type") == "assistant":
            for b in m["message"]["content"]:
                if b["type"] == "tool_use": tools.append({"name": b["name"], "input": b["input"]})
                if b["type"] == "text": text = b["text"]
        if m.get("type") == "user" and isinstance(m["message"]["content"], list):
            errors += sum(1 for b in m["message"]["content"] if b.get("type") == "tool_result" and b.get("is_error"))
        if m.get("type") == "result": result = m
    answer = result.get("result", text) or ""
    u = result.get("usage", {})
    sides = [s for s in (path_of(t) for t in tools) if s]
    return {"qid": qid, "phrasing": phrasing, "condition": cname, "model": init.get("model"),
            "mcp_status": [s.get("status") for s in init.get("mcp_servers", [])],
            "skill_listed": "search-notes" in init.get("skills", []),
            "tools": tools, "sides": sides, "first": sides[0] if sides else None,
            "used_mcp": "mcp" in sides, "used_skill": "skill" in sides,
            "used_toolsearch": any(t["name"] == "ToolSearch" for t in tools), "tool_errors": errors,
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
                for attempt in range(3):   # a Claude Code auto-update mid-run makes launches exit at once with code 2
                    r = run(q["id"], q, phrasing, cname, c)
                    if r["model"]: break
                    time.sleep(30)
                f.write(json.dumps(r) + "\n"); f.flush()
                print(f"q{q['id']:>2} {phrasing:6} {cname:12} first={r['first']!s:5} sides={r['sides']} ok={r['correct']!s:5} "
                      f"${r['cost_usd']} {r['wall_s']}s", flush=True)
print("DONE", flush=True)
