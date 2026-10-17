#!/usr/bin/env python3
"""Context cost of each side: input tokens for a one-turn prompt that needs no tools, with nothing installed,
the MCP server, the Skill, or both, with tool search on and off.
Usage: python3 bench/overhead.py <scratch-folder> <out.json> [model]   (run after run.py, which creates the folders)"""
import json, os, subprocess, sys
from pathlib import Path

CLEAN = {"CLAUDE_CODE_DISABLE_CLAUDE_MDS": "1", "CLAUDE_CODE_DISABLE_AUTO_MEMORY": "1"}
root = Path(__file__).resolve().parent.parent
scratch, out = (Path(a).resolve() for a in sys.argv[1:3])
model = sys.argv[3] if len(sys.argv) > 3 else "sonnet"
corpus = scratch  # the server only needs a folder; no search happens in these runs
work = {False: scratch / "work-plain", True: scratch / "work-skill"}

def claude(name, mcp, skill, claude_env=None):
    servers = {"notes": {"command": "node", "args": [str(root / "server.mjs"), str(corpus)]}} if mcp else {}
    cfg = scratch / f"mcp-overhead-{name}.json"
    cfg.write_text(json.dumps({"mcpServers": servers}))
    cmd = ["claude", "-p", "Reply with the single word OK.", "--model", model, "--mcp-config", str(cfg),
           "--strict-mcp-config", "--setting-sources", "project", "--disallowedTools", "WebSearch", "WebFetch",
           "--allowedTools", "mcp__notes", "Skill", "Bash(node:*)", "--output-format", "json"]
    p = subprocess.run(cmd, cwd=work[skill], env={**os.environ, **CLEAN, **(claude_env or {})}, capture_output=True,
                       text=True, stdin=subprocess.DEVNULL, timeout=300)
    r = json.loads(p.stdout)
    u = r.get("usage", {})
    return {"name": name, "input_tokens": u.get("input_tokens", 0) + u.get("cache_read_input_tokens", 0)
            + u.get("cache_creation_input_tokens", 0), "turns": r.get("num_turns"), "answer": r.get("result")}

off = {"ENABLE_TOOL_SEARCH": "false"}
rows = []
for rep in range(3):
    rows += [claude("none", False, False), claude("mcp", True, False), claude("skill", False, True),
             claude("both", True, True), claude("none-upfront", False, False, off),
             claude("mcp-upfront", True, False, off), claude("both-upfront", True, True, off)]
out.write_text(json.dumps(rows, indent=2))
for r in rows: print(r["name"], r["input_tokens"], r["turns"], (r["answer"] or "")[:40])
