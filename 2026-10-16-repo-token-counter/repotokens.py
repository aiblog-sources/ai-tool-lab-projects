#!/usr/bin/env python3
"""Count the tokens in a repository and estimate what sending it to a model costs.

Exact counts: OpenAI's o200k_base (tiktoken) and, if a tokenizer.json is given, an
open-weight tokenizer such as Qwen3. Claude has no offline tokenizer, so Claude counts
are estimates: o200k_base tokens times a per-file-type ratio measured with
measure_claude.py (see claude_ratios.json).
"""
import argparse
import fnmatch
import json
import os
import subprocess
import sys
from collections import defaultdict
from pathlib import Path

import tiktoken

HERE = Path(__file__).resolve().parent
MAX_FILE_BYTES = 2_000_000

# File types used to pick a Claude ratio. Anything else falls back to "other".
KINDS = {
    ".py": "python", ".js": "javascript", ".mjs": "javascript", ".cjs": "javascript",
    ".ts": "typescript", ".tsx": "typescript", ".go": "go", ".rs": "rust",
    ".md": "markdown", ".rst": "markdown", ".txt": "markdown",
    ".json": "data", ".yml": "data", ".yaml": "data", ".toml": "data", ".lock": "data",
    ".csv": "csv", ".svg": "svg",
}
LOCKFILES = {"package-lock.json", "yarn.lock", "pnpm-lock.yaml", "Cargo.lock", "poetry.lock",
             "uv.lock", "go.sum", "composer.lock", "Gemfile.lock"}


def kind_of(path):
    if Path(path).name in LOCKFILES:
        return "lockfile"
    return KINDS.get(Path(path).suffix.lower(), "other")


def list_files(root):
    """Tracked files when root is a git checkout (respects .gitignore), else a plain walk."""
    try:
        out = subprocess.run(["git", "-C", str(root), "ls-files", "-z"], capture_output=True,
                             check=True).stdout
        return [p for p in out.decode().split("\0") if p]
    except (subprocess.CalledProcessError, FileNotFoundError):
        files = []
        for dirpath, dirnames, filenames in os.walk(root):
            dirnames[:] = [d for d in dirnames if not d.startswith(".") and d != "node_modules"]
            for f in filenames:
                files.append(os.path.relpath(os.path.join(dirpath, f), root))
        return files


def read_text(path):
    """Return the file as text, or None for binaries, huge files and non-UTF-8 files."""
    try:
        if path.stat().st_size > MAX_FILE_BYTES:
            return None
        data = path.read_bytes()
    except OSError:
        return None
    if b"\0" in data[:8192]:
        return None
    try:
        return data.decode("utf-8")
    except UnicodeDecodeError:
        return None


def count_repo(root, hf_tokenizer=None, include_lockfiles=True, exclude=()):
    enc = tiktoken.get_encoding("o200k_base")
    hf = None
    if hf_tokenizer:
        from tokenizers import Tokenizer
        hf = Tokenizer.from_file(hf_tokenizer)
    rows, skipped = [], 0
    for rel in list_files(root):
        if not include_lockfiles and Path(rel).name in LOCKFILES:
            continue
        if any(fnmatch.fnmatch(rel, pat) or fnmatch.fnmatch(Path(rel).name, pat) for pat in exclude):
            continue
        text = read_text(Path(root) / rel)
        if text is None:
            skipped += 1
            continue
        row = {"path": rel, "kind": kind_of(rel), "lockfile": Path(rel).name in LOCKFILES,
               "chars": len(text), "o200k": len(enc.encode(text, disallowed_special=()))}
        if hf:
            row["hf"] = len(hf.encode(text, add_special_tokens=False).ids)
        rows.append(row)
    return rows, skipped


def claude_estimate(rows, ratios):
    return round(sum(r["o200k"] * ratios.get(r["kind"], ratios["other"]) for r in rows))


def fmt_usd(x):
    return f"${x:,.2f}" if x >= 0.01 else f"${x:,.4f}"


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("path", nargs="?", default=".")
    ap.add_argument("--hf-tokenizer", help="path to a Hugging Face tokenizer.json to count with as well")
    ap.add_argument("--no-lockfiles", action="store_true", help="leave lockfiles out of the totals")
    ap.add_argument("--exclude", action="append", default=[], metavar="GLOB",
                    help="leave out files matching GLOB (path or file name), e.g. '*.svg'; repeatable")
    ap.add_argument("--top", type=int, default=5, help="largest files to list")
    ap.add_argument("--json", action="store_true", help="print the totals as JSON")
    args = ap.parse_args()

    ratios = json.loads((HERE / "claude_ratios.json").read_text())["ratio_to_o200k"]
    prices = json.loads((HERE / "prices.json").read_text())
    rows, skipped = count_repo(args.path, args.hf_tokenizer, not args.no_lockfiles, args.exclude)
    if not rows:
        sys.exit("no text files found")

    totals = {"files": len(rows), "skipped": skipped, "chars": sum(r["chars"] for r in rows),
              "o200k": sum(r["o200k"] for r in rows),
              "claude_est": claude_estimate(rows, ratios["claude"]),
              "claude_legacy_est": claude_estimate(rows, ratios["claude_legacy"])}
    if args.hf_tokenizer:
        totals["hf"] = sum(r["hf"] for r in rows)
    totals["chars_div_4"] = totals["chars"] // 4
    if args.json:
        print(json.dumps(totals, indent=2))
        return

    print(f"{totals['files']} text files ({skipped} binary or skipped), {totals['chars']:,} characters\n")
    by_kind = defaultdict(lambda: [0, 0, 0])
    for r in rows:
        k = by_kind[r["kind"]]
        k[0] += 1
        k[1] += r["chars"]
        k[2] += r["o200k"]
    print(f"{'type':<12}{'files':>7}{'chars':>12}{'o200k':>10}{'chars/tok':>11}")
    for kind, (n, c, t) in sorted(by_kind.items(), key=lambda kv: -kv[1][2]):
        print(f"{kind:<12}{n:>7}{c:>12,}{t:>10,}{c / max(t, 1):>11.2f}")

    print(f"\nTokens: o200k_base (GPT-5 family) {totals['o200k']:,}", end="")
    if args.hf_tokenizer:
        print(f", {Path(args.hf_tokenizer).parent.parent.parent.name.replace('models--', '').replace('--', '/')} {totals['hf']:,}", end="")
    print(f",\n        Claude 4.7 and later (estimate) {totals['claude_est']:,}, "
          f"Claude 4.6 and earlier (estimate) {totals['claude_legacy_est']:,}, chars/4 rule {totals['chars_div_4']:,}")

    top = sorted(rows, key=lambda r: -r["o200k"])[: args.top]
    print("\nLargest files (o200k tokens):")
    for r in top:
        share = 100 * r["o200k"] / totals["o200k"]
        print(f"  {r['o200k']:>9,}  {share:5.1f}%  {r['path']}{'  (lockfile)' if r['lockfile'] else ''}")
    if top[0]["o200k"] > totals["o200k"] / 4:
        print(f"  One file is over a quarter of the total; if a model does not need it, try --exclude "
              f"'{Path(top[0]['path']).name}'{' or --no-lockfiles' if top[0]['lockfile'] else ''}.")

    print(f"\nCost to send the whole repo once as input (prices checked {prices['checked']}):")
    for m in prices["models"]:
        tokens = {"claude": totals["claude_est"], "claude_legacy": totals["claude_legacy_est"]}.get(
            m["tokenizer"], totals["o200k"])
        price = m["input_per_mtok"]
        note = ""
        if m.get("long_context_over") and tokens > m["long_context_over"]:
            price = m["long_context_input_per_mtok"]
            note = f" (over {m['long_context_over'] // 1000}k: long-context price)"
        fits = "" if tokens <= m.get("context", tokens) else f"  does not fit in {m['context'] // 1000}k context"
        print(f"  {m['name']:<24}{fmt_usd(tokens * price / 1e6):>10}  at ${price}/MTok{note}{fits}")
    print("\nClaude counts are estimates (see claude_ratios.json); the other prices use o200k_base,\n"
          "which is exact for the GPT-5 family and an approximation for newer GPT and Gemini models.")


if __name__ == "__main__":
    main()
