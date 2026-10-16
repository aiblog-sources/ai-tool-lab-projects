#!/usr/bin/env python3
"""Send a whole repo (every text file repotokens.py counts, joined by newlines) to Claude
once per model and record the real token count, to check the estimate on repos the
ratios were not measured on.

usage: validate_claude.py OUT.json REPO_DIR [REPO_DIR ...]
"""
import json
import sys
from pathlib import Path

from measure_claude import PREFIX, QUESTION, claude_input_tokens
from repotokens import list_files, read_text

MODELS = ["claude-sonnet-5-5", "claude-sonnet-4-6"]


def main():
    out, repos = sys.argv[1], sys.argv[2:]
    results = []
    for repo in repos:
        texts = [t for t in (read_text(Path(repo) / rel) for rel in list_files(repo)) if t is not None]
        packed = "\n".join(texts)
        for model in MODELS:
            base = claude_input_tokens(model, PREFIX + QUESTION)
            total = claude_input_tokens(model, PREFIX + packed + "\n\n" + QUESTION)
            results.append({"repo": Path(repo).name, "model": model, "files": len(texts),
                            "chars": len(packed), "claude_tokens": total - base})
            print(results[-1], file=sys.stderr)
    Path(out).write_text(json.dumps(results, indent=2) + "\n")


if __name__ == "__main__":
    main()
