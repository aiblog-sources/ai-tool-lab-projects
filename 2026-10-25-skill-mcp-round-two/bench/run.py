#!/usr/bin/env python3
"""Round two of Skill vs MCP in Claude Code: ask claude -p the same 12 questions under each condition in
conditions.json and record every tool call.

Usage: python3 bench/run.py <corpus-folder> <scratch-folder> <out.jsonl>
A condition says which MCP server is connected (real names, vague names or none), which Skill sits in the project
(the script Skill, a "bridge" Skill that points at the MCP tool, or none), whether the script Skill comes from a
plugin instead, and the model. CONDITIONS=a,b and QIDS=1,2 limit a run.
"""
import json, os, re, subprocess, sys, time
from pathlib import Path

# No CLAUDE.md files and no auto-memory: only the prompt, the built-in tools, the Skill and the MCP server.
CLEAN = {"CLAUDE_CODE_DISABLE_CLAUDE_MDS": "1", "CLAUDE_CODE_DISABLE_AUTO_MEMORY": "1"}
VAGUE = {"SEARCH_NAME": "query", "SEARCH_DESC": "Runs a query.", "READ_NAME": "get", "READ_DESC": "Gets an item by path."}

root = Path(__file__).resolve().parent.parent
corpus, scratch, out = (Path(a).resolve() for a in sys.argv[1:4])
scratch.mkdir(parents=True, exist_ok=True)
questions = json.loads((root / "bench/questions.json").read_text())
conditions = json.loads((root / "bench/conditions.json").read_text())
only = os.environ.get("CONDITIONS")
if only:
    conditions = {k: v for k, v in conditions.items() if k in only.split(",")}
qids = os.environ.get("QIDS")
if qids:
    questions = [q for q in questions if str(q["id"]) in qids.split(",")]
# Round one left Claude Code's read-only file tools on. In the smoke test of this round, a run with the vague server and
# no Skill grepped the whole home folder for notes, so file browsing outside the two search paths is denied here.
DENY = ["WebSearch", "WebFetch", "Read", "Grep", "Glob", "LS"] + [f"Bash({c}:*)" for c in
        ("grep", "egrep", "rg", "find", "ls", "cat", "head", "tail", "cd", "tree", "awk", "sed", "less", "file", "locate", "du", "stat", "wc")]
phrasings = {"hinted": "Check my notes. {q}", "bare": "{q}"}

# One working folder per Skill variant; each holds nothing but .claude/skills/search-notes.
def workdir(skill):
    w = scratch / f"work-{skill or 'plain'}"
    w.mkdir(parents=True, exist_ok=True)
    if skill:
        link = w / ".claude/skills/search-notes"
        link.parent.mkdir(parents=True, exist_ok=True)
        if not link.exists():
            link.symlink_to(root / "skills" / skill)
    return w

done = set()
if out.exists():
    for line in out.read_text().splitlines():
        r = json.loads(line); done.add((r["qid"], r["phrasing"], r["condition"]))

def search_side(t):
    """Which side ran a search or read: the MCP server or the Skill's script. The Skill tool itself only loads text."""
    if t["name"].startswith("mcp__"): return "mcp"
    if t["name"] == "Bash" and "search.mjs" in t["input"].get("command", ""): return "script"
    return None

def run(qid, q, phrasing, cname, c):
    servers = {}
    if c["mcp"] == "real":
        servers["notes"] = {"command": "node", "args": [str(root / "server.mjs"), str(corpus)], "env": {}}
    elif c["mcp"] == "vague":
        servers["s1"] = {"command": "node", "args": [str(root / "server.mjs"), str(corpus)], "env": VAGUE}
    cfg = scratch / f"mcp-{cname}.json"
    cfg.write_text(json.dumps({"mcpServers": servers}))
    prompt = phrasings[phrasing].format(q=q["q"]) + " Answer in one or two sentences."
    # Same permissions in every condition: both MCP server names, the Skill tool and `node` in Bash.
    cmd = ["claude", "-p", prompt, "--model", c["model"], "--mcp-config", str(cfg), "--strict-mcp-config",
           "--setting-sources", "project", "--disallowedTools", *DENY,
           "--allowedTools", "mcp__notes", "mcp__s1", "Skill", "Bash(node:*)", "--output-format", "stream-json", "--verbose"]
    if c.get("plugin"):
        cmd += ["--plugin-dir", str(root / "plugin")]
    env = {**os.environ, **CLEAN, "NOTES_DIR": str(corpus)}
    t0 = time.time()
    p = subprocess.run(cmd, cwd=workdir(c.get("skill")), env=env, capture_output=True, text=True,
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
    sides = [s for s in (search_side(t) for t in tools) if s]
    skill_calls = [t["input"].get("skill") or t["input"].get("command") for t in tools if t["name"] == "Skill"]
    return {"qid": qid, "phrasing": phrasing, "condition": cname, "model": init.get("model"),
            "mcp_status": [s.get("status") for s in init.get("mcp_servers", [])],
            "skills_listed": [s for s in init.get("skills", []) if "search-notes" in s],
            "plugins": init.get("plugins"),
            "tools": tools, "sides": sides, "first": sides[0] if sides else None,
            "skill_invoked": bool(skill_calls), "skill_calls": skill_calls,
            "skill_before_search": bool(skill_calls) and (not sides or
                next(i for i, t in enumerate(tools) if t["name"] == "Skill") <
                next(i for i, t in enumerate(tools) if search_side(t))),
            "used_toolsearch": any(t["name"] == "ToolSearch" for t in tools), "tool_errors": errors,
            "answer": answer, "correct": all(re.search(e, answer, re.I) for e in q["expect"]),
            "turns": result.get("num_turns"), "cost_usd": result.get("total_cost_usd"),
            "duration_ms": result.get("duration_ms"), "wall_s": round(wall, 1),
            "input_tokens": u.get("input_tokens", 0) + u.get("cache_read_input_tokens", 0) + u.get("cache_creation_input_tokens", 0),
            "output_tokens": u.get("output_tokens"), "exit": p.returncode}

with out.open("a") as f:
    for q in questions:              # conditions interleaved per question, so drift hits them alike
        for phrasing in phrasings:
            for cname, c in conditions.items():
                if (q["id"], phrasing, cname) in done: continue
                for attempt in range(3):   # a Claude Code auto-update mid-run makes launches exit at once with code 2
                    r = run(q["id"], q, phrasing, cname, c)
                    if r["model"]: break
                    time.sleep(30)
                f.write(json.dumps(r) + "\n"); f.flush()
                print(f"q{q['id']:>2} {phrasing:6} {cname:20} first={r['first']!s:6} skill={r['skill_invoked']!s:5} "
                      f"sides={r['sides']} ok={r['correct']!s:5} ${r['cost_usd']} {r['wall_s']}s", flush=True)
print("DONE", flush=True)
