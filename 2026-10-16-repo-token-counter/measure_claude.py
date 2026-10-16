#!/usr/bin/env python3
"""Measure how many tokens Claude models use for real files, with `claude -p`.

Claude has no offline tokenizer. Every `claude -p --output-format json` run reports the
input tokens it used, so: tokens(file) = input(prefix + file + question) - input(prefix + question).
The system prompt is replaced with one short line and tools are off, so the baseline is
small and stable. Writes one CSV row per (file, model).

usage: measure_claude.py OUT.csv REPO_DIR [REPO_DIR ...]
       measure_claude.py OUT.csv REPO_DIR::RELATIVE_PATH [...]   (exact files instead of a sample)
"""
import csv
import hashlib
import json
import os
import subprocess
import sys
import time
from pathlib import Path

import tiktoken

from repotokens import kind_of, list_files, read_text

MODELS = ["claude-haiku-5-5", "claude-sonnet-5-5", "claude-opus-5-5", "claude-opus-4-7",
          "claude-sonnet-4-6"]
QUESTION = "Reply with OK."
# Every prompt starts with this line. A prompt that starts with "/" (a file opening with a
# // comment) makes claude -p send about 300 fewer tokens of its own context, which broke
# the subtraction in a first run (results/discarded/).
PREFIX = "Here is a file.\n\n"
CODE_KINDS = {"python", "javascript", "typescript", "go", "rust"}
ENV = dict(os.environ, CLAUDE_CODE_DISABLE_CLAUDE_MDS="1", CLAUDE_CODE_DISABLE_AUTO_MEMORY="1")


def claude_input_tokens(model, prompt):
    """Total input tokens (uncached + cache write + cache read) of one claude -p run."""
    for attempt in range(3):
        p = subprocess.run(
            ["claude", "-p", "--model", model, "--system-prompt", "You reply with OK.",
             "--tools", "", "--strict-mcp-config", "--setting-sources", "",
             "--output-format", "json"],
            input=prompt, capture_output=True, text=True, env=ENV, cwd="/tmp", timeout=300)
        try:
            d = json.loads(p.stdout)
            u = d["usage"]
            if model not in d.get("modelUsage", {}):
                raise ValueError(f"answered by {list(d.get('modelUsage', {}))}")
            return (u["input_tokens"] + u.get("cache_creation_input_tokens", 0)
                    + u.get("cache_read_input_tokens", 0))
        except (ValueError, KeyError, json.JSONDecodeError) as e:
            print(f"  retry {model} ({e}; exit {p.returncode})", file=sys.stderr)
            time.sleep(10)
    raise RuntimeError(f"claude -p failed 3 times for {model}")


def pick_sample(repo):
    """Up to 4 code files, 1 Markdown and 1 data/config file, 1-40k characters, chosen by path hash."""
    picked = {"code": [], "markdown": [], "data": []}
    limits = {"code": 4, "markdown": 1, "data": 1}
    for rel in sorted(list_files(repo), key=lambda p: hashlib.sha1(p.encode()).hexdigest()):
        kind = kind_of(rel)
        group = "code" if kind in CODE_KINDS else kind
        if group not in picked or len(picked[group]) >= limits[group]:
            continue
        text = read_text(Path(repo) / rel)
        if text is None or not (1_000 <= len(text) <= 40_000):
            continue
        picked[group].append((rel, kind, text))
    return picked["code"] + picked["markdown"] + picked["data"]


def main():
    out, repos = sys.argv[1], sys.argv[2:]
    enc = tiktoken.get_encoding("o200k_base")
    sample = []
    for arg in repos:
        if "::" in arg:
            repo, rel = arg.split("::", 1)
            sample.append((Path(repo).name, rel, kind_of(rel), read_text(Path(repo) / rel)))
        else:
            sample += [(Path(arg).name, rel, kind, text) for rel, kind, text in pick_sample(arg)]
    print(f"{len(sample)} files, {sum(len(t) for *_, t in sample):,} characters", file=sys.stderr)
    with open(out, "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["model", "repo", "path", "kind", "chars", "o200k", "claude_tokens",
                    "baseline_before", "baseline_after"])
        for model in MODELS:
            base = claude_input_tokens(model, PREFIX + QUESTION)
            rows = []
            for repo, rel, kind, text in sample:
                total = claude_input_tokens(model, PREFIX + text + "\n\n" + QUESTION)
                rows.append([model, repo, rel, kind, len(text),
                             len(enc.encode(text, disallowed_special=())), total - base])
                print(f"  {model} {repo}/{rel}: {total - base}", file=sys.stderr)
            after = claude_input_tokens(model, PREFIX + QUESTION)
            for r in rows:
                w.writerow(r + [base, after])
            f.flush()


if __name__ == "__main__":
    main()
