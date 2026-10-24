#!/usr/bin/env python3
"""Count how many function and method definitions survive in each --compress pack.

ky: TypeScript methods and functions, split into `#private` class methods and the rest.
requests: Python `def` lines. diff-tour: Go `func` declarations.
usage: analyze_packs.py REPOS_DIR PACKS_DIR
"""
import re
import sys
from pathlib import Path

repos, packs = Path(sys.argv[1]), Path(sys.argv[2])
# A definition line: optional modifiers, then the name and an opening parenthesis.
TS_PRIV = re.compile(r"^\s*(?:static\s+)?(?:async\s+)?#(\w+)\s*\(", re.M)
PY_DEF = re.compile(r"^\s*(?:async\s+)?def\s+(\w+)", re.M)
GO_FUNC = re.compile(r"^func\s+(?:\([^)]*\)\s*)?(\w+)", re.M)


def files(pack):
    """Yield (path, text) for each <file> in a Repomix XML pack."""
    for m in re.finditer(r'<file path="([^"]+)">\n(.*?)</file>', pack.read_text(), re.S):
        yield m.group(1), m.group(2)


def count(pattern, texts):
    return sum(len(pattern.findall(t)) for t in texts)


print("| Repo | What is counted | In the source | In the --compress pack | Kept |")
print("| --- | --- | --- | --- | --- |")
for repo, pat, label in [("ky", TS_PRIV, "`#private` methods"),
                         ("requests", PY_DEF, "`def` (functions and methods)"),
                         ("diff-tour", GO_FUNC, "`func` declarations")]:
    full = [t for _, t in files(packs / f"{repo}-full.xml")]
    comp = [t for _, t in files(packs / f"{repo}-compress.xml")]
    a, b = count(pat, full), count(pat, comp)
    print(f"| {repo} | {label} | {a} | {b} | {b / a:.0%} |")

# ky's Ky class in detail: which private methods are in the source and which in the pack.
full = dict(files(packs / "ky-full.xml"))["source/core/Ky.ts"]
comp = dict(files(packs / "ky-compress.xml"))["source/core/Ky.ts"]
names, kept = TS_PRIV.findall(full), set(TS_PRIV.findall(comp))
print(f"\nky source/core/Ky.ts: {len(names)} `#private` methods in the source, "
      f"{len(kept)} in the --compress pack, "
      f"{len(full.splitlines())} lines -> {len(comp.splitlines())} lines.")
print("Private methods missing from the pack: " + ", ".join(f"`#{n}`" for n in names if n not in kept))

# Go: which declarations are missing.
full = [t for _, t in files(packs / "diff-tour-full.xml")]
comp = " ".join(t for _, t in files(packs / "diff-tour-compress.xml"))
miss = [n for t in full for n in GO_FUNC.findall(t) if not re.search(rf"^func\s+(?:\([^)]*\)\s*)?{n}\b", comp, re.M)]
print("diff-tour declarations missing from the pack: " + (", ".join(miss) or "none"))
