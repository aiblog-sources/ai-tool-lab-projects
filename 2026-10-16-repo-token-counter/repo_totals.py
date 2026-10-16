#!/usr/bin/env python3
"""Count every tracked text file of several repos with several tokenizers.

usage: repo_totals.py OUT.csv QWEN3_TOKENIZER_JSON DEEPSEEK_V3_TOKENIZER_JSON REPO_DIR [REPO_DIR ...]
"""
import csv
import subprocess
import sys
import time
from pathlib import Path

import tiktoken
from tokenizers import Tokenizer

from repotokens import LOCKFILES, kind_of, list_files, read_text


def main():
    out, qwen_path, ds_path, repos = sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4:]
    tok = {
        "o200k": lambda t, e=tiktoken.get_encoding("o200k_base"): len(e.encode(t, disallowed_special=())),
        "cl100k": lambda t, e=tiktoken.get_encoding("cl100k_base"): len(e.encode(t, disallowed_special=())),
        "qwen3": lambda t, e=Tokenizer.from_file(qwen_path): len(e.encode(t, add_special_tokens=False).ids),
        "deepseek_v3": lambda t, e=Tokenizer.from_file(ds_path): len(e.encode(t, add_special_tokens=False).ids),
    }
    with open(out, "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["repo", "commit", "path", "kind", "lockfile", "chars", "bytes"] + list(tok) + ["o200k_seconds"])
        for repo in repos:
            commit = subprocess.run(["git", "-C", repo, "rev-parse", "--short", "HEAD"],
                                    capture_output=True, text=True).stdout.strip()
            for rel in list_files(repo):
                text = read_text(Path(repo) / rel)
                if text is None:
                    continue
                counts = {}
                for name, fn in tok.items():
                    t0 = time.perf_counter()
                    counts[name] = fn(text)
                    if name == "o200k":
                        secs = time.perf_counter() - t0
                w.writerow([Path(repo).name, commit, rel, kind_of(rel), Path(rel).name in LOCKFILES,
                            len(text), len(text.encode())] + list(counts.values()) + [f"{secs:.6f}"])


if __name__ == "__main__":
    main()
