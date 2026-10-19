#!/usr/bin/env python3
"""Ask Claude the same questions with a repo packed four ways and record the answers.

Conditions: none (no repo, the model's memory), full, nocomments (--remove-comments
--remove-empty-lines) and compress (--compress). One `claude -p` run per question, tools off,
system prompt replaced with one line, CLAUDE.md and auto-memory disabled. Appends one JSON
line per run to OUT and skips runs already in it, so it can be restarted.

usage: run.py PACKS_DIR OUT.jsonl [--model M] [--reps N] [--workers N]
"""
import argparse
import json
import os
import subprocess
import sys
import tempfile
import threading
import time
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

HERE = Path(__file__).parent
CONDITIONS = ["none", "full", "nocomments", "compress"]
SYSTEM = "You answer questions about source code briefly and precisely."
ENV = dict(os.environ, CLAUDE_CODE_DISABLE_CLAUDE_MDS="1", CLAUDE_CODE_DISABLE_AUTO_MEMORY="1")
lock = threading.Lock()


def build_prompt(q, repo, cond, packs):
    if cond == "none":
        return (f"Question about {repo['name']}, current main branch:\n{q['q']}\n\n"
                "Answer in one short line. If you do not know, reply exactly: NOT SURE")
    pack = (packs / f"{q['repo']}-{cond}.xml").read_text()
    # Fixed first line: a prompt starting with "/" changes what claude -p sends.
    return ("Here is a repository packed into one file by Repomix.\n\n" + pack +
            f"\n\nQuestion about the repository above:\n{q['q']}\n\n"
            "Answer in one short line, using only the repository content above. "
            "If it does not contain the answer, reply exactly: NOT IN CONTEXT")


def ask(model, prompt, workdir):
    for attempt in range(4):
        t0 = time.time()
        p = subprocess.run(
            ["claude", "-p", "--model", model, "--system-prompt", SYSTEM, "--tools", "",
             "--strict-mcp-config", "--setting-sources", "", "--output-format", "json"],
            input=prompt, capture_output=True, text=True, env=ENV, cwd=workdir, timeout=600)
        try:
            d = json.loads(p.stdout)
            if model not in d.get("modelUsage", {}) or d.get("is_error"):
                raise ValueError(f"model {list(d.get('modelUsage', {}))} error {d.get('is_error')}")
            d["wall_s"] = round(time.time() - t0, 2)
            return d
        except (ValueError, json.JSONDecodeError) as e:
            print(f"  retry ({e}; exit {p.returncode}) {p.stdout[:200]!r}", file=sys.stderr)
            time.sleep(20 * (attempt + 1))
    raise RuntimeError("claude -p failed 4 times")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("packs", type=Path)
    ap.add_argument("out", type=Path)
    ap.add_argument("--model", default="claude-sonnet-5-5")
    ap.add_argument("--reps", type=int, default=2)
    ap.add_argument("--workers", type=int, default=2)
    ap.add_argument("--limit", type=int, default=0, help="only the first N runs (smoke test)")
    a = ap.parse_args()
    spec = json.load(open(HERE / "questions.json"))
    done = set()
    if a.out.exists():
        for line in a.out.read_text().splitlines():
            r = json.loads(line)
            done.add((r["id"], r["condition"], r["rep"], r["model"]))
    # Same repo and condition back to back, so the packed file stays in the prompt cache.
    jobs = [(rep, cond, q) for rep in range(1, a.reps + 1)
            for repo in spec["repos"] for cond in CONDITIONS
            for q in spec["questions"] if q["repo"] == repo
            if (q["id"], cond, rep, a.model) not in done]
    if a.limit:
        jobs = jobs[:a.limit]
    print(f"{len(jobs)} runs to go", file=sys.stderr)
    workdir = tempfile.mkdtemp(prefix="repomix-bench-")

    def one(job):
        rep, cond, q = job
        d = ask(a.model, build_prompt(q, spec["repos"][q["repo"]], cond, a.packs), workdir)
        u = d["usage"]
        row = {"id": q["id"], "repo": q["repo"], "condition": cond, "rep": rep, "model": a.model,
               "answer": d["result"].strip(),
               "input_tokens": u["input_tokens"] + u.get("cache_creation_input_tokens", 0)
               + u.get("cache_read_input_tokens", 0),
               "cache_read": u.get("cache_read_input_tokens", 0),
               "output_tokens": u["output_tokens"], "cost_usd": d.get("total_cost_usd"),
               "duration_ms": d.get("duration_ms"), "wall_s": d["wall_s"],
               "claude_code": d.get("version"), "ts": time.strftime("%Y-%m-%dT%H:%M:%S")}
        with lock:
            with open(a.out, "a") as f:
                f.write(json.dumps(row) + "\n")
            print(f"{rep} {cond:10} {q['id']:20} {row['input_tokens']:7} {row['answer'][:70]!r}",
                  file=sys.stderr)

    with ThreadPoolExecutor(a.workers) as ex:
        list(ex.map(one, jobs))
    print("DONE", file=sys.stderr)


if __name__ == "__main__":
    main()
