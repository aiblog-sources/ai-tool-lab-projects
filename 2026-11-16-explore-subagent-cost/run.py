#!/usr/bin/env python3
"""Ask Claude Code architecture questions about real checkouts, with and without the Explore subagent.

Conditions (main model Sonnet 5.5 in every one):
  default        tools on, Claude decides whether to delegate
  no-subagent    --disallowedTools Agent (no Agent/Task tool at all)
  explore        the prompt asks for the Explore subagent (it inherits the main model)
  explore-haiku  as explore, plus CLAUDE_CODE_SUBAGENT_MODEL=claude-haiku-5-5 and CLAUDE_CODE_SUBAGENT_MODEL_FORCE=1

One `claude -p --output-format stream-json --verbose` run per question, cwd = the repo checkout, no user/project
settings, no MCP, CLAUDE.md and auto-memory disabled. The raw stream is kept in STREAMS_DIR (not published: the
init event names local paths); one summary JSON line per run goes to OUT. Restartable: runs already in OUT are skipped.

usage: run.py REPOS_DIR STREAMS_DIR OUT.jsonl [--reps N] [--workers N] [--conditions a,b] [--limit N]
       [--questions questions-broad.json]
"""
import argparse
import json
import os
import subprocess
import sys
import threading
import time
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

HERE = Path(__file__).parent
MODEL = "claude-sonnet-5-5"
CONDITIONS = ["default", "no-subagent", "explore", "explore-haiku"]
BASE_ENV = dict(os.environ, CLAUDE_CODE_DISABLE_CLAUDE_MDS="1", CLAUDE_CODE_DISABLE_AUTO_MEMORY="1")
lock = threading.Lock()


def build(q, repo, cond):
    fmt = q.get("format", "Answer in at most 150 words and name the functions involved.")
    prompt = f"Question about this repository ({repo['name']}):\n{q['q']}\n\n{fmt}"
    if cond in ("explore", "explore-haiku"):
        prompt += " Use the Explore subagent to search the code."
    args = ["claude", "-p", "--model", MODEL, "--strict-mcp-config", "--setting-sources", "",
            "--output-format", "stream-json", "--verbose"]
    if cond == "no-subagent":
        args += ["--disallowedTools", "Agent"]
    env = dict(BASE_ENV)
    if cond == "explore-haiku":
        env.update(CLAUDE_CODE_SUBAGENT_MODEL="claude-haiku-5-5", CLAUDE_CODE_SUBAGENT_MODEL_FORCE="1")
    return prompt, args, env


def ctx(u):
    return u.get("input_tokens", 0) + u.get("cache_read_input_tokens", 0) + u.get("cache_creation_input_tokens", 0)


def summarize(events):
    """Turn a stream into numbers. Main = events without parent_tool_use_id; sub = inside a subagent."""
    s = {"agent_calls": [], "main_tools": 0, "sub_tools": 0, "main_calls": 0, "sub_calls": 0,
         "main_ctx_final": 0, "main_ctx_max": 0, "main_input_total": 0, "sub_input_total": 0,
         "sub_models": set(), "results": 0}
    seen = set()  # one API message can arrive as several events (one per content block)
    result = None
    for e in events:
        t = e.get("type")
        if t == "assistant":
            m = e["message"]
            sub = bool(e.get("parent_tool_use_id"))
            for c in m.get("content", []):
                if c.get("type") == "tool_use":
                    if sub:
                        s["sub_tools"] += 1
                    else:
                        s["main_tools"] += 1
                        if c["name"] in ("Agent", "Task"):
                            s["agent_calls"].append(c["input"].get("subagent_type", "?"))
            if m.get("id") in seen:
                continue
            seen.add(m.get("id"))
            n = ctx(m.get("usage", {}))
            if sub:
                s["sub_calls"] += 1
                s["sub_input_total"] += n
                s["sub_models"].add(m.get("model"))
            else:
                s["main_calls"] += 1
                s["main_input_total"] += n
                s["main_ctx_final"] = n
                s["main_ctx_max"] = max(s["main_ctx_max"], n)
        elif t == "system" and e.get("subtype") == "init" and "version" not in s:
            s["version"] = e.get("claude_code_version")
        elif t == "result":
            s["results"] += 1
            result = e
    s["sub_models"] = sorted(x for x in s["sub_models"] if x)
    return s, result


def ask(prompt, args, env, cwd, stream_path):
    for attempt in range(4):
        t0 = time.time()
        p = subprocess.run(args, input=prompt, capture_output=True, text=True, env=env, cwd=cwd, timeout=900)
        wall = round(time.time() - t0, 2)
        events = []
        for line in p.stdout.splitlines():
            try:
                events.append(json.loads(line))
            except json.JSONDecodeError:
                pass
        s, result = summarize(events)
        if result and not result.get("is_error") and MODEL in result.get("modelUsage", {}):
            stream_path.write_text(p.stdout)
            return s, result, wall
        print(f"  retry {attempt + 1} (exit {p.returncode}) {p.stderr[-300:]!r}", file=sys.stderr)
        time.sleep(30 * (attempt + 1))
    raise RuntimeError("claude -p failed 4 times")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("repos", type=Path)
    ap.add_argument("streams", type=Path)
    ap.add_argument("out", type=Path)
    ap.add_argument("--reps", type=int, default=2)
    ap.add_argument("--workers", type=int, default=3)
    ap.add_argument("--conditions", default=",".join(CONDITIONS))
    ap.add_argument("--limit", type=int, default=0)
    ap.add_argument("--questions", default="questions.json")
    a = ap.parse_args()
    a.streams.mkdir(parents=True, exist_ok=True)
    spec = json.load(open(HERE / a.questions))
    done = set()
    if a.out.exists():
        for line in a.out.read_text().splitlines():
            r = json.loads(line)
            done.add((r["id"], r["condition"], r["rep"]))
    # Interleave conditions so time-of-night effects spread evenly.
    jobs = [(rep, cond, q) for rep in range(1, a.reps + 1) for q in spec["questions"]
            for cond in a.conditions.split(",") if (q["id"], cond, rep) not in done]
    if a.limit:
        jobs = jobs[:a.limit]
    print(f"{len(jobs)} runs to go", file=sys.stderr)

    def one(job):
        rep, cond, q = job
        prompt, args, env = build(q, spec["repos"][q["repo"]], cond)
        s, res, wall = ask(prompt, args, env, a.repos / q["repo"], a.streams / f"{q['id']}.{cond}.{rep}.jsonl")
        mu = res["modelUsage"]
        row = {"id": q["id"], "repo": q["repo"], "condition": cond, "rep": rep,
               "answer": res["result"].strip(), **s,
               "cost_usd": round(res["total_cost_usd"], 6),
               "cost_by_model": {k: round(v["costUSD"], 6) for k, v in mu.items()},
               "tokens_by_model": {k: v["inputTokens"] + v["cacheReadInputTokens"] + v["cacheCreationInputTokens"]
                                   for k, v in mu.items()},
               "output_by_model": {k: v["outputTokens"] for k, v in mu.items()},
               "num_turns": res.get("num_turns"), "wall_s": wall,
               "ts": time.strftime("%Y-%m-%dT%H:%M:%S")}
        with lock:
            with open(a.out, "a") as f:
                f.write(json.dumps(row) + "\n")
            print(f"{rep} {cond:13} {q['id']:22} agents={row['agent_calls']} ${row['cost_usd']:.3f} "
                  f"ctx={row['main_ctx_final']} {wall:.0f}s", file=sys.stderr)

    with ThreadPoolExecutor(a.workers) as ex:
        list(ex.map(one, jobs))
    print("DONE", file=sys.stderr)


if __name__ == "__main__":
    main()
