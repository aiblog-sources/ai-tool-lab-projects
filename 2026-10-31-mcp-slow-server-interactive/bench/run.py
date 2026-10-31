#!/usr/bin/env python3
"""Drive interactive Claude Code in tmux with an MCP server that takes N seconds to connect.

For each run: start `claude` (TUI, not -p) in a detached tmux pane, wait until the input box is drawn, type the
question at once (or after TYPE_DELAY seconds), wait for the turn to end, and if the notes tool was not called,
send "Please try again." as a second turn. Everything measured comes from three places:
  - the server's own event log (EVENT_LOG): process start, connected, tool calls, wall-clock ms;
  - Claude Code's session transcript (~/.claude/projects/*/<session>.jsonl): the tool list of every API request
    (`api-request-shape`), `deferred_tools_delta` attachments (`pendingMcpServers`), tool calls, answers;
  - two pane captures (2 s after sending, and at the end) for what the user sees.
Only derived fields are saved; the transcript itself stays local (it holds the user's own settings and skills).

Usage: python3 bench/run.py <corpus> <work-folder> <out.jsonl> [reps]
Env: DELAYS=0,5000,15000,40000  CONDS=deferred,per-tool,per-server  MODEL=sonnet  TYPE_DELAY=0
     CLAUDE_ENV="NAME=value;NAME2=value" extra environment for Claude Code itself (recorded in each row)
"""
import glob, json, os, re, subprocess, sys, time, uuid
from pathlib import Path

HERE = Path(__file__).resolve().parent
SERVER = HERE.parent / "server.mjs"
QUESTION = ("What generation speed in tokens per second did Gemma 4 E2B reach on the 2-core laptop? "
            "Answer in one or two sentences.")
RETRY = "Please try again."
RIGHT = re.compile(r"\b7\.1\b")          # the corpus says 7.1 tokens/s
CLEAN = {"CLAUDE_CODE_DISABLE_CLAUDE_MDS": "1", "CLAUDE_CODE_DISABLE_AUTO_MEMORY": "1"}
BLOCKED = ["Bash", "Read", "Grep", "Glob", "Task", "Monitor", "Workflow", "WebSearch", "WebFetch"]
CONDS = {
    "deferred":   {"env": {}, "config": {}},
    "per-tool":   {"env": {"ALWAYS_LOAD": "search"}, "config": {}},
    "per-server": {"env": {}, "config": {"alwaysLoad": True}},
}
TOOL = "mcp__notes__search_notes"

def sh(*a): return subprocess.run(a, capture_output=True, text=True).stdout

def pane(s): return sh("tmux", "capture-pane", "-p", "-t", s)

def transcript(sid):
    f = glob.glob(os.path.expanduser(f"~/.claude/projects/*/{sid}.jsonl"))
    return [json.loads(l) for l in open(f[0])] if f else []

def turns_done(sid): return sum(1 for m in transcript(sid) if m.get("type") == "system" and m.get("subtype") == "turn_duration")

def ms(ts):   # transcript ISO time -> epoch ms
    from datetime import datetime
    return round(datetime.fromisoformat(ts.replace("Z", "+00:00")).timestamp() * 1000)

def wait(pred, timeout, step=0.1):
    t0 = time.time()
    while time.time() - t0 < timeout:
        if pred(): return True
        time.sleep(step)
    return False

def ready(text):
    return "Accessing workspace" not in text and any(l.startswith("❯") for l in text.splitlines())

def prompts(sid): return sum(1 for m in transcript(sid) if m.get("type") == "user" and isinstance(m["message"].get("content"), str))

def send(s, sid, text):
    """Type, pause, Enter. Enter sent in the same burst as the text is taken as part of a paste and dropped,
    so wait 0.4 s and press Enter again until the prompt shows up in the transcript."""
    n = prompts(sid)
    subprocess.run(["tmux", "send-keys", "-t", s, "-l", text])
    for _ in range(5):
        time.sleep(0.4)
        subprocess.run(["tmux", "send-keys", "-t", s, "Enter"])
        if wait(lambda: prompts(sid) > n, 2, 0.1): return True
    return False

def parse(sid, t0):
    """Per turn: API requests (time, notes tool in the request?), pending servers, tool calls, final text."""
    turns, cur, shapes, reqs = [], None, {}, []
    for m in transcript(sid):
        t = m.get("type")
        if t == "user" and isinstance(m["message"].get("content"), str):
            cur = {"prompt_ms": ms(m["timestamp"]) - t0, "requests": [], "pending": [], "tools": [], "text": ""}
            turns.append(cur)
        if cur is None: continue
        # `api-request-shape` is written only when the request shape changes; `api-request` names its shapeHash
        if t == "api-request-shape":
            shapes[m["shapeHash"]] = [x.get("name") for x in m["shape"].get("tools", [])]
        if t == "api-request" and m.get("querySource") == "repl_main_thread":
            names = shapes.get(m.get("shapeHash"), [])
            reqs.append({"ms": ms(m["timestamp"]) - t0, "search_in_request": TOOL in names,
                                    "notes_tools": sorted(n for n in names if n.startswith("mcp__notes__"))})
        if t == "attachment" and m["attachment"].get("type") == "deferred_tools_delta":
            a = m["attachment"]
            cur["pending"].append({"ms": ms(m["timestamp"]) - t0, "pending": a.get("pendingMcpServers"),
                                   "failed": a.get("failedMcpServers"),
                                   "added_notes": [n for n in a.get("addedNames", []) if n.startswith("mcp__notes__")]})
        if t == "assistant":
            for b in m["message"]["content"]:
                if b["type"] == "tool_use": cur["tools"].append({"name": b["name"], "input": b["input"]})
                if b["type"] == "text": cur["text"] = b["text"]
    for r in reqs:   # the transcript can write a turn's first request before its prompt line: assign by time
        owner = [c for c in turns if c["prompt_ms"] <= r["ms"]]
        if owner: owner[-1]["requests"].append(r)
    for c in turns:
        c["called_search"] = any(x["name"] == TOOL for x in c["tools"])
        c["right"] = bool(RIGHT.search(c["text"]))
    return turns

def run_one(corpus, work, scratch, cond, delay, model, type_delay, claude_env):
    sid, s = str(uuid.uuid4()), f"slow-{os.getpid()}"
    ev = scratch / f"ev-{sid}.jsonl"
    c = CONDS[cond]
    cfg = scratch / f"mcp-{sid}.json"
    cfg.write_text(json.dumps({"mcpServers": {"notes": {
        "command": "node", "args": [str(SERVER), str(corpus)],
        "env": {**c["env"], "STARTUP_DELAY_MS": str(delay), "EVENT_LOG": str(ev)}, **c["config"]}}}))
    env = " ".join(f"{k}={v}" for k, v in {**CLEAN, **claude_env}.items())
    cmd = (f"env {env} claude --model {model} --session-id {sid} --permission-mode default "
           f"--mcp-config {cfg} --strict-mcp-config --setting-sources project "
           f"--disallowedTools {' '.join(BLOCKED)} --allowedTools mcp__notes")
    t0 = round(time.time() * 1000)
    subprocess.run(["tmux", "new-session", "-d", "-s", s, "-x", "120", "-y", "40", "-c", str(work), cmd])
    row = {"condition": cond, "claude_env": claude_env, "delay_ms": delay, "type_delay_s": type_delay, "model": model, "session": sid}
    try:
        if not wait(lambda: ready(pane(s)), 60):
            row["error"] = "input box never appeared"; return row
        row["ready_ms"] = round(time.time() * 1000) - t0
        time.sleep(type_delay)
        row["typed_ms"] = round(time.time() * 1000) - t0
        if not send(s, sid, QUESTION): row["error"] = "prompt not submitted"; return row
        row["sent_ms"] = round(time.time() * 1000) - t0
        time.sleep(2)
        row["pane_after_send"] = scrub(pane(s))
        if not wait(lambda: turns_done(sid) >= 1, 240, 0.5):
            row["error"] = "turn 1 did not finish"; row["pane_end"] = scrub(pane(s)); return row
        row["turn1_done_ms"] = round(time.time() * 1000) - t0
        t1 = parse(sid, t0)
        if not t1 or not t1[0]["called_search"]:
            time.sleep(1)
            send(s, sid, RETRY)
            row["retry_sent_ms"] = round(time.time() * 1000) - t0
            if not wait(lambda: turns_done(sid) >= 2, 240, 0.5): row["error"] = "turn 2 did not finish"
        time.sleep(1)
        row["pane_end"] = scrub(pane(s))
        row["turns"] = parse(sid, t0)
    finally:
        subprocess.run(["tmux", "kill-session", "-t", s], capture_output=True)
        time.sleep(1)
        row["server_events"] = [{**{k: v for k, v in e.items() if k != "pid"}, "ms": e["t"] - t0}
                                for e in map(json.loads, ev.read_text().splitlines())] if ev.exists() else []
        for e in row["server_events"]: e.pop("t", None)
    return row

HOME = str(Path.home())
def scrub(text):
    return text.replace(HOME, "~").replace(os.environ.get("USER", "\0"), "<user>")

def main():
    corpus, work, out = Path(sys.argv[1]).resolve(), Path(sys.argv[2]).resolve(), Path(sys.argv[3])
    reps = int(sys.argv[4]) if len(sys.argv) > 4 else 2
    scratch = work.parent / "scratch"; scratch.mkdir(exist_ok=True)
    delays = [int(x) for x in os.environ.get("DELAYS", "0,5000,15000,40000").split(",")]
    conds = os.environ.get("CONDS", ",".join(CONDS)).split(",")
    model, type_delay = os.environ.get("MODEL", "sonnet"), float(os.environ.get("TYPE_DELAY", "0"))
    claude_env = dict(kv.split("=", 1) for kv in os.environ.get("CLAUDE_ENV", "").split(";") if kv)
    for rep in range(reps):
        for delay in delays:
            for cond in conds:
                r = {"rep": rep + 1, **run_one(corpus, work, scratch, cond, delay, model, type_delay, claude_env)}
                with open(out, "a") as f: f.write(json.dumps(r) + "\n")
                t = r.get("turns") or [{}]
                conn = next((e["ms"] for e in r["server_events"] if e["event"] == "connected"), None)
                print(rep + 1, delay, cond, "sent", r.get("sent_ms"), "connected", conn,
                      "t1 search" if t[0].get("called_search") else "t1 MISS",
                      "right" if t[0].get("right") else "", "| t2", (t[1]["called_search"], t[1]["right"]) if len(t) > 1 else "-",
                      r.get("error", ""), flush=True)

if __name__ == "__main__":
    main()
