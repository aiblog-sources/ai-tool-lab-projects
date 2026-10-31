#!/usr/bin/env python3
"""Same question, same slow server, but `claude -p` (headless). Does the first turn wait here?
Usage: python3 bench/headless.py <corpus> <work-folder> <out.jsonl> [reps]   Env: DELAYS, CONDS, MODEL"""
import json, os, subprocess, sys, time, uuid
from pathlib import Path
sys.path.insert(0, os.path.dirname(__file__))
from run import CLEAN, BLOCKED, CONDS, QUESTION, RIGHT, SERVER, TOOL

corpus, work, out = Path(sys.argv[1]).resolve(), Path(sys.argv[2]).resolve(), Path(sys.argv[3])
reps = int(sys.argv[4]) if len(sys.argv) > 4 else 2
scratch = work.parent / "scratch"; scratch.mkdir(exist_ok=True)
for rep in range(reps):
    for delay in [int(x) for x in os.environ.get("DELAYS", "15000").split(",")]:
        for cond in os.environ.get("CONDS", "deferred,per-tool").split(","):
            sid = str(uuid.uuid4()); ev = scratch / f"ev-{sid}.jsonl"; cfg = scratch / f"mcp-{sid}.json"
            c = CONDS[cond]
            cfg.write_text(json.dumps({"mcpServers": {"notes": {"command": "node", "args": [str(SERVER), str(corpus)],
                "env": {**c["env"], "STARTUP_DELAY_MS": str(delay), "EVENT_LOG": str(ev)}, **c["config"]}}}))
            t0 = time.time() * 1000
            p = subprocess.run(["claude", "-p", QUESTION, "--model", os.environ.get("MODEL", "sonnet"), "--session-id", sid,
                                "--mcp-config", str(cfg), "--strict-mcp-config", "--setting-sources", "project",
                                "--disallowedTools", *BLOCKED, "--allowedTools", "mcp__notes", "--output-format", "json"],
                               cwd=work, env={**os.environ, **CLEAN}, capture_output=True, text=True, timeout=300,
                               stdin=subprocess.DEVNULL)
            wall = time.time() * 1000 - t0
            res = json.loads(p.stdout) if p.stdout.strip().startswith("{") else {}
            evs = [json.loads(l) for l in ev.read_text().splitlines()] if ev.exists() else []
            calls = [e for e in evs if e["event"] == "call"]
            row = {"rep": rep + 1, "mode": "headless", "condition": cond, "delay_ms": delay, "wall_ms": round(wall),
                   "connected_ms": next((round(e["t"] - t0) for e in evs if e["event"] == "connected"), None),
                   "first_call_ms": round(calls[0]["t"] - t0) if calls else None, "called_search": bool(calls),
                   "right": bool(RIGHT.search(res.get("result", ""))), "answer": res.get("result", "")[:300],
                   "cost_usd": res.get("total_cost_usd"), "exit": p.returncode}
            with open(out, "a") as f: f.write(json.dumps(row) + "\n")
            print(rep + 1, delay, cond, row["connected_ms"], row["first_call_ms"], row["right"], row["wall_ms"], flush=True)
