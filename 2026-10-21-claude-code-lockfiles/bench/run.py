#!/usr/bin/env python3
"""Does Claude Code read package-lock.json, and what keeps it out?

Each run copies the fixture repo (a small Express API with a real 131 KB package-lock.json) to a fresh
folder, applies one condition, asks one question with `claude -p`, and saves the stream-json events.

Conditions:
  none         nothing
  claudeignore a .claudeignore file listing lockfiles (a popular tip; not a documented Claude Code feature)
  dotignore    a .ignore file listing lockfiles (ripgrep's ignore file, which Claude Code's Grep and Glob use)
  deny         permissions.deny Read rules for lockfiles in .claude/settings.json (the documented way)
  instruction  "do not read lockfiles" appended to the system prompt (stands in for a CLAUDE.md rule)
  deny_npmls   deny rules + `--allowedTools "Bash(npm ls:*)"` + one line saying to use `npm ls --package-lock-only`.
               (A first batch put the allow rule in .claude/settings.json: Claude Code 2.1.306 ignores project
               allow rules in a workspace that was never trusted, while it applies the deny rules. Those 12 runs
               are kept in results as deny_npmls_untrusted.)

usage: run.py OUT_DIR [--reps N] [--jobs N] [--only COND,COND] [--model MODEL]
"""
import argparse
import json
import os
import shutil
import subprocess
import sys
import tempfile
import time
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

HERE = Path(__file__).resolve().parent
FIXTURE = HERE.parent / "fixture"
LOCK_PATTERNS = ["package-lock.json", "yarn.lock", "pnpm-lock.yaml"]
INSTRUCTION = ("Never read lockfiles (package-lock.json, yarn.lock, pnpm-lock.yaml) in this project. "
               "They are large generated files; use package.json instead.")
NPMLS_HINT = ("Lockfiles are blocked. For installed versions or why a package is in the tree, run "
              "`npm ls <name> --package-lock-only` (works without node_modules).")
CONDITIONS = ["none", "claudeignore", "dotignore", "deny", "instruction", "deny_npmls"]
QUESTIONS = json.loads((HERE / "questions.json").read_text())
ENV = dict(os.environ, CLAUDE_CODE_DISABLE_CLAUDE_MDS="1", CLAUDE_CODE_DISABLE_AUTO_MEMORY="1")


def prepare(cond, work):
    repo = work / "orders-api"
    shutil.copytree(FIXTURE, repo)
    if cond in ("claudeignore", "dotignore"):
        name = ".claudeignore" if cond == "claudeignore" else ".ignore"
        (repo / name).write_text("\n".join(LOCK_PATTERNS) + "\n")
    if cond in ("deny", "deny_npmls"):
        (repo / ".claude").mkdir()
        rules = [f"Read(./{p})" for p in LOCK_PATTERNS] + [f"Read(**/{p})" for p in LOCK_PATTERNS]
        (repo / ".claude" / "settings.json").write_text(
            json.dumps({"permissions": {"deny": rules}}, indent=2) + "\n")
    git = ["git", "-c", "user.name=fixture", "-c", "user.email=fixture@example.com"]
    subprocess.run(["git", "init", "-q"], cwd=repo, check=True)
    subprocess.run(git + ["add", "-A"], cwd=repo, check=True)
    subprocess.run(git + ["commit", "-qm", "fixture"], cwd=repo, check=True)
    return repo


def run_one(job):
    cond, q, rep, out_dir, model = job
    out = out_dir / f"{cond}__{q['id']}__{rep}.json"
    if out.exists():
        return out
    work = Path(tempfile.mkdtemp(prefix="lockbench-"))
    try:
        repo = prepare(cond, work)
        cmd = ["claude", "-p", q["prompt"], "--model", model, "--output-format", "stream-json",
               "--verbose", "--setting-sources", "project", "--strict-mcp-config",
               "--permission-mode", "acceptEdits",
               "--allowedTools", "Read", "Grep", "Glob", "Edit", "Write",
               "--disallowedTools", "Task", "Agent", "WebSearch", "WebFetch", "Monitor", "Workflow"]
        if cond == "instruction":
            cmd += ["--append-system-prompt", INSTRUCTION]
        if cond == "deny_npmls":
            cmd += ["--append-system-prompt", NPMLS_HINT]
            cmd[cmd.index("Write") + 1:cmd.index("Write") + 1] = ["Bash(npm ls:*)"]
        for attempt in range(3):
            t0 = time.time()
            p = subprocess.run(cmd, cwd=repo, capture_output=True, text=True, env=ENV, timeout=900,
                               stdin=subprocess.DEVNULL)
            events = []
            for line in p.stdout.splitlines():
                try:
                    events.append(json.loads(line))
                except json.JSONDecodeError:
                    pass
            if any(e.get("type") == "result" for e in events):
                break
            print(f"  retry {cond} {q['id']} {rep} (exit {p.returncode})", file=sys.stderr)
            time.sleep(15)
        files_after = {}
        for rel in q.get("check_files", []):
            f = repo / rel
            files_after[rel] = f.read_text() if f.exists() else None
        out.write_text(json.dumps({
            "condition": cond, "question": q["id"], "rep": rep, "model": model,
            "wall_seconds": round(time.time() - t0, 1), "exit": p.returncode,
            "work_dir": str(repo), "files_after": files_after, "events": events,
        }))
        print(f"done {cond} {q['id']} {rep} in {time.time() - t0:.0f}s", file=sys.stderr)
        return out
    finally:
        shutil.rmtree(work, ignore_errors=True)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("out_dir")
    ap.add_argument("--reps", type=int, default=2)
    ap.add_argument("--jobs", type=int, default=3)
    ap.add_argument("--only", default=",".join(CONDITIONS[:5]))
    ap.add_argument("--model", default="claude-sonnet-5-5")
    a = ap.parse_args()
    out_dir = Path(a.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    conds = a.only.split(",")
    # Interleave: every condition gets each question before any repeat starts.
    jobs = [(c, q, r, out_dir, a.model) for r in range(1, a.reps + 1) for q in QUESTIONS for c in conds]
    with ThreadPoolExecutor(a.jobs) as ex:
        list(ex.map(run_one, jobs))
    print("DONE", file=sys.stderr)


if __name__ == "__main__":
    main()
