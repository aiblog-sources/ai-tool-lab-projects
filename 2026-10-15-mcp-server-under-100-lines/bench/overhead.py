#!/usr/bin/env python3
"""Context cost of the server: input tokens for a one-turn prompt with no server, deferred tools, upfront tools,
and what Claude Code reports at startup for the noisy servers.
Usage: python3 bench/overhead.py <corpus> <work> <out.json> [model]"""
import json, os, subprocess, sys
from pathlib import Path

# No CLAUDE.md files and no auto-memory: only the prompt, the built-in tools and this MCP server.
CLEAN = {"CLAUDE_CODE_DISABLE_CLAUDE_MDS": "1", "CLAUDE_CODE_DISABLE_AUTO_MEMORY": "1"}
here = Path(__file__).resolve().parent
corpus, work, out = (Path(a).resolve() for a in sys.argv[1:4])
model = sys.argv[4] if len(sys.argv) > 4 else "sonnet"

def claude(name, servers, prompt, claude_env=None):
    cfg = work.parent / f"mcp-{name}.json"
    cfg.write_text(json.dumps({"mcpServers": servers}))
    cmd = ["claude", "-p", prompt, "--model", model, "--mcp-config", str(cfg), "--strict-mcp-config",
           "--setting-sources", "project", "--disallowedTools", "WebSearch", "WebFetch",
           "--allowedTools", "mcp__notes", "--output-format", "stream-json", "--verbose"]
    p = subprocess.run(cmd, cwd=work, env={**os.environ, **CLEAN, **(claude_env or {})}, capture_output=True, text=True,
                       stdin=subprocess.DEVNULL, timeout=300)
    init, result, tools = {}, {}, []
    for line in p.stdout.splitlines():
        try: m = json.loads(line)
        except json.JSONDecodeError: continue
        if m.get("type") == "system" and m.get("subtype") == "init": init = m
        if m.get("type") == "assistant":
            tools += [{"name": b["name"], "input": b["input"]} for b in m["message"]["content"] if b["type"] == "tool_use"]
        if m.get("type") == "user":
            for b in m["message"]["content"] if isinstance(m["message"]["content"], list) else []:
                if b.get("type") == "tool_result": tools.append({"result": str(b.get("content"))[:300], "is_error": b.get("is_error")})
        if m.get("type") == "result": result = m
    u = result.get("usage", {})
    return {"name": name, "mcp_servers": init.get("mcp_servers"), "n_tools": len(init.get("tools", [])),
            "input_tokens": u.get("input_tokens", 0) + u.get("cache_read_input_tokens", 0) + u.get("cache_creation_input_tokens", 0),
            "turns": result.get("num_turns"), "answer": result.get("result"), "tools": tools, "cost_usd": result.get("total_cost_usd")}

srv = lambda script, env=None: {"notes": {"command": "node", "args": [str(script), str(corpus)], "env": env or {}}}
real, noisy = here.parent / "server.mjs", here / "noisy-server.mjs"
ok = "Reply with the single word OK."
ask = "Check my notes. What generation speed in tokens per second did Gemma 4 E2B reach on the 2-core laptop? Answer in one sentence."
off = {"ENABLE_TOOL_SEARCH": "false"}
rows = []
for rep in range(3):
    rows += [claude("none", {}, ok), claude("deferred", srv(real), ok),
             claude("none-upfront", {}, ok, off), claude("upfront", srv(real), ok, off)]
for rep in range(int(os.environ.get("NOISY_REPS", "1"))):
    for mode in ["start", "each", "nonl", "early-stdin"]:
        rows += [claude(f"noisy-{mode}", srv(noisy, {"NOISY": mode}), ask),
                 claude(f"noisy-{mode}-upfront", srv(noisy, {"NOISY": mode}), ask, off)]
out.write_text(json.dumps(rows, indent=2))
for r in rows: print(r["name"], r["mcp_servers"], r["n_tools"], r["input_tokens"], r["turns"], (r["answer"] or "")[:150].replace("\n", " "))
