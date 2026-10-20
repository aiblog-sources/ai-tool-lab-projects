"""One `claude -p` launch with one MCP server, parsed from stream-json. Shared by run.py, overhead.py and slow.py."""
import json, os, subprocess, time
from pathlib import Path

# No CLAUDE.md files and no auto-memory: only the prompt, the built-in tools and this MCP server.
CLEAN = {"CLAUDE_CODE_DISABLE_CLAUDE_MDS": "1", "CLAUDE_CODE_DISABLE_AUTO_MEMORY": "1"}
# Without these, vague-tool runs grepped the whole home folder for the answer (and could reach the corpus).
# The MCP server is the only way to the notes, so "used the tool" is the only path to a right answer.
BLOCKED = ["Bash", "Read", "Grep", "Glob", "Task", "Monitor", "Workflow", "WebSearch", "WebFetch"]
SERVER = Path(__file__).resolve().parent.parent / "server.mjs"

def claude(prompt, cname, c, corpus, work, model="sonnet"):
    """c: {"server": name, "env": server env, "config": extra server-entry keys, "claude_env": Claude Code env}.
    c["server"] may be None for a run with no MCP server at all."""
    cfg = Path(work).parent / f"mcp-{cname}.json"
    servers = {} if not c.get("server") else {c["server"]: {"command": "node", "args": [str(SERVER), str(corpus)],
                                                           "env": c.get("env", {}), **c.get("config", {})}}
    cfg.write_text(json.dumps({"mcpServers": servers}))
    stream = c.get("stream_input")   # the Agent SDK's mode: prompt on stdin as JSON, servers connect in background
    cmd = ["claude", "-p", *([] if stream else [prompt]), "--model", model, "--mcp-config", str(cfg), "--strict-mcp-config",
           "--setting-sources", "project", "--disallowedTools", *BLOCKED,
           "--allowedTools", f"mcp__{c.get('server') or 'none'}", "--output-format", "stream-json", "--verbose"]
    if stream: cmd += ["--input-format", "stream-json"]
    env = {**os.environ, **CLEAN, **c.get("claude_env", {})}
    t0 = time.time()
    p = subprocess.run(cmd, cwd=work, env=env, capture_output=True, text=True, timeout=300,
                       **({"input": json.dumps({"type": "user", "message": {"role": "user", "content": prompt}}) + "\n"}
                          if stream else {"stdin": subprocess.DEVNULL}))
    wall = time.time() - t0
    tools, text, result, init, first_in, t_init, t_first = [], "", {}, {}, None, None, None
    tot = lambda u: u.get("input_tokens", 0) + u.get("cache_read_input_tokens", 0) + u.get("cache_creation_input_tokens", 0)
    for line in p.stdout.splitlines():
        try: m = json.loads(line)
        except json.JSONDecodeError: continue
        if m.get("type") == "system" and m.get("subtype") == "init": init = m
        if m.get("type") == "assistant":
            if first_in is None: first_in = tot(m["message"].get("usage", {}))   # context of the first API request
            for b in m["message"]["content"]:
                if b["type"] == "tool_use": tools.append({"id": b["id"], "name": b["name"], "input": b["input"]})
                if b["type"] == "text": text = b["text"]
        if m.get("type") == "user" and isinstance(m["message"].get("content"), list):
            for b in m["message"]["content"]:
                if b.get("type") == "tool_result":
                    tools.append({"result_for": b.get("tool_use_id"), "is_error": bool(b.get("is_error")),
                                  "text": str(b.get("content"))[:200]})
        if m.get("type") == "result": result = m
    results = [t for t in tools if "result_for" in t]
    tools = [t for t in tools if "result_for" not in t]
    u = result.get("usage", {})
    names = [t["name"] for t in tools]
    first_mcp = next((i for i, n in enumerate(names) if n.startswith("mcp__")), None)
    return {"condition": cname, "model": init.get("model"), "session_id": init.get("session_id"),
            "mcp_status": [s.get("status") for s in init.get("mcp_servers", [])],
            "init_tools": [t for t in init.get("tools", []) if t.startswith("mcp__")],
            "tools": tools, "results": results, "used_mcp": first_mcp is not None,
            "used_toolsearch": "ToolSearch" in names,
            # ToolSearch before the first MCP call means the tool was not in context yet when Claude wanted it
            "searched_first": first_mcp is not None and "ToolSearch" in names[:first_mcp],
            "builtin": [n for n in names if not n.startswith("mcp__") and n != "ToolSearch"],
            "answer": result.get("result", text) or "", "turns": result.get("num_turns"),
            "cost_usd": result.get("total_cost_usd"), "duration_ms": result.get("duration_ms"),
            "duration_api_ms": result.get("duration_api_ms"), "wall_s": round(wall, 2),
            "first_input_tokens": first_in, "input_tokens": tot(u), "output_tokens": u.get("output_tokens"),
            "exit": p.returncode, "stderr": p.stderr[-500:]}

def claude_retry(*a, **k):
    for attempt in range(3):   # a Claude Code auto-update mid-run made launches exit at once with code 2
        r = claude(*a, **k)
        if r["model"]: return r
        time.sleep(30)
    return r
