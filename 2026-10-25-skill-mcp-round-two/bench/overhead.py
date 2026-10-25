#!/usr/bin/env python3
"""Idle context cost of the Skill as a project Skill and as a plugin Skill: input tokens for a one-turn prompt that
needs no tools. Usage: python3 bench/overhead.py <scratch-folder> <out.json> [model]   (run after run.py)"""
import json, os, subprocess, sys
from pathlib import Path

CLEAN = {"CLAUDE_CODE_DISABLE_CLAUDE_MDS": "1", "CLAUDE_CODE_DISABLE_AUTO_MEMORY": "1"}
root = Path(__file__).resolve().parent.parent
scratch, out = (Path(a).resolve() for a in sys.argv[1:3])
model = sys.argv[3] if len(sys.argv) > 3 else "sonnet"

def claude(name, work, plugin=False):
    cfg = scratch / "mcp-overhead.json"
    cfg.write_text(json.dumps({"mcpServers": {}}))
    cmd = ["claude", "-p", "Reply with the single word OK.", "--model", model, "--mcp-config", str(cfg),
           "--strict-mcp-config", "--setting-sources", "project", "--output-format", "json"]
    if plugin:
        cmd += ["--plugin-dir", str(root / "plugin")]
    p = subprocess.run(cmd, cwd=scratch / work, env={**os.environ, **CLEAN}, capture_output=True,
                       text=True, stdin=subprocess.DEVNULL, timeout=300)
    r = json.loads(p.stdout)
    u = r.get("usage", {})
    return {"name": name, "input_tokens": u.get("input_tokens", 0) + u.get("cache_read_input_tokens", 0)
            + u.get("cache_creation_input_tokens", 0), "turns": r.get("num_turns"), "answer": r.get("result")}

rows = []
for rep in range(3):
    rows += [claude("none", "work-plain"), claude("project-skill", "work-script"),
             claude("bridge-skill", "work-bridge"), claude("plugin-skill", "work-plain", plugin=True)]
out.write_text(json.dumps(rows, indent=2))
for r in rows: print(r["name"], r["input_tokens"], r["turns"], (r["answer"] or "")[:40])
