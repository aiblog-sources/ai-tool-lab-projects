#!/usr/bin/env python3
"""Count how many Claude tokens a lockfile costs, with `claude -p` (no API key needed).

tokens(file) = input(prefix + file + question) - input(prefix + question), the same subtraction as
2026-10-16-repo-token-counter/measure_claude.py. Claude Opus 4.7 and later (Haiku, Sonnet and Opus 5.5)
share one tokenizer, so one model is enough. Also prints OpenAI o200k_base and characters / 4.

usage: lock_tokens.py OUT.csv FILE [FILE ...]
"""
import csv
import json
import os
import subprocess
import sys
import time
from pathlib import Path

import tiktoken

MODEL = "claude-sonnet-5-5"
PREFIX = "Here is a file.\n\n"   # never start a prompt with "/" (see the 2026-10-16 project)
QUESTION = "Reply with OK."
ENV = dict(os.environ, CLAUDE_CODE_DISABLE_CLAUDE_MDS="1", CLAUDE_CODE_DISABLE_AUTO_MEMORY="1")


def claude_input_tokens(prompt):
    for _ in range(3):
        p = subprocess.run(
            ["claude", "-p", "--model", MODEL, "--system-prompt", "You reply with OK.", "--tools", "",
             "--strict-mcp-config", "--setting-sources", "", "--output-format", "json"],
            input=prompt, capture_output=True, text=True, env=ENV, cwd="/tmp", timeout=600)
        try:
            d = json.loads(p.stdout)
            if MODEL not in d.get("modelUsage", {}):
                raise ValueError(f"answered by {list(d.get('modelUsage', {}))}")
            u = d["usage"]
            return (u["input_tokens"] + u.get("cache_creation_input_tokens", 0)
                    + u.get("cache_read_input_tokens", 0))
        except (ValueError, KeyError, json.JSONDecodeError) as e:
            print(f"  retry ({e}; exit {p.returncode})", file=sys.stderr)
            time.sleep(10)
    raise RuntimeError("claude -p failed 3 times")


def main():
    out, files = sys.argv[1], sys.argv[2:]
    enc = tiktoken.get_encoding("o200k_base")
    base = claude_input_tokens(PREFIX + QUESTION)
    rows = []
    for f in files:
        text = Path(f).read_text()
        total = claude_input_tokens(PREFIX + text + "\n\n" + QUESTION)
        rows.append([Path(f).name, len(text.encode()), text.count("\n"), len(text) // 4,
                     len(enc.encode(text, disallowed_special=())), total - base])
        print(f"  {Path(f).name}: {total - base}", file=sys.stderr)
    after = claude_input_tokens(PREFIX + QUESTION)
    with open(out, "w", newline="") as fh:
        w = csv.writer(fh)
        w.writerow(["file", "bytes", "lines", "chars_div_4", "o200k", "claude", "baseline_before",
                    "baseline_after"])
        for r in rows:
            w.writerow(r + [base, after])


if __name__ == "__main__":
    main()
