#!/usr/bin/env python3
"""Size of each packed file, per-file compress ratios, and repeated text inside --compress output.

Repomix --compress joins the code chunks tree-sitter captured with a "⋮----" separator. A chunk
that is wholly contained in another chunk of the same file is text the model reads twice.

usage: analyze_packs.py PACKS_DIR   (needs tiktoken for o200k counts)
"""
import re
import sys
from pathlib import Path

import tiktoken

enc = tiktoken.get_encoding("o200k_base")
packs = Path(sys.argv[1])
FILE = re.compile(r'<file path="([^"]+)">\n(.*?)\n</file>', re.S)


def files(repo, mode):
    return dict(FILE.findall((packs / f"{repo}-{mode}.xml").read_text()))


def tok(s):
    return len(enc.encode(s, disallowed_special=()))


print("| Repo | Mode | Characters | o200k tokens | vs full |")
print("| --- | --- | --- | --- | --- |")
for repo in ["ky", "requests", "cobra"]:
    base = tok((packs / f"{repo}-full.xml").read_text())
    for mode in ["full", "nocomments", "compress"]:
        text = (packs / f"{repo}-{mode}.xml").read_text()
        t = tok(text)
        print(f"| {repo} | {mode} | {len(text):,} | {t:,} | {t / base:.2f} |")

print("\nFiles that --compress made larger, and text repeated inside compressed files:\n")
print("| Repo | File | Full tokens | Compress tokens | Ratio | Repeated chunk tokens |")
print("| --- | --- | --- | --- | --- | --- |")
totals = {}
for repo in ["ky", "requests", "cobra"]:
    full, comp = files(repo, "full"), files(repo, "compress")
    rep_total = 0
    for path, body in comp.items():
        chunks = [c.strip() for c in body.split("⋮----") if c.strip()]
        repeated = sum(tok(a) for i, a in enumerate(chunks)
                       if any(i != j and a in b for j, b in enumerate(chunks)))
        rep_total += repeated
        f, c = tok(full[path]), tok(body)
        if c > f or repeated > 200:
            print(f"| {repo} | {path} | {f:,} | {c:,} | {c / f:.2f} | {repeated:,} |")
    totals[repo] = (rep_total, sum(tok(b) for b in comp.values()))
print()
for repo, (r, t) in totals.items():
    print(f"{repo}: {r:,} of {t:,} compressed tokens are repeated chunks ({r / t:.0%})")

# Function definitions that survive --compress, per file, counted by name.
DEF = {"requests": re.compile(r"^[ \t]*(?:async[ \t]+)?def (\w+)\(", re.M),
       "cobra": re.compile(r"^func (?:\([^)]*\)\s*)?(\w+)\(", re.M)}
print("\nFunction definitions kept by --compress:\n")
for repo, rx in DEF.items():
    full, comp = files(repo, "full"), files(repo, "compress")
    n_full = n_kept = multi = multi_kept = 0
    for path, body in full.items():
        kept = rx.findall(comp.get(path, ""))
        for m in rx.finditer(body):
            line = body[m.start():body.find("\n", m.start())]
            one_line = line.count("(") <= line.count(")")
            n_full += 1
            ok = m.group(1) in kept
            if ok:
                kept.remove(m.group(1))
                n_kept += 1
            if not one_line:
                multi += 1
                multi_kept += ok
    print(f"{repo}: {n_kept} of {n_full} kept; signatures over several lines: {multi_kept} of {multi} kept "
          "(matched by name, so a same-named definition elsewhere in the file counts as kept)")
