#!/usr/bin/env python3
"""Turn the raw measurements in results/ into claude_ratios.json and results/summary.md.

usage: analyze.py QWEN3_TOKENIZER_JSON XENOVA_CLAUDE_TOKENIZER_JSON REPOS_DIR
"""
import csv
import json
import sys
from collections import defaultdict
from pathlib import Path

from tokenizers import Tokenizer

from repotokens import count_repo

HERE = Path(__file__).resolve().parent
RES = HERE / "results"
NEW, LEGACY = "claude-sonnet-5-5", "claude-sonnet-4-6"


def load(name):
    p = RES / name
    return list(csv.DictReader(open(p))) if p.exists() else []


def main():
    qwen, xenova = Tokenizer.from_file(sys.argv[1]), Tokenizer.from_file(sys.argv[2])
    repos_dir = Path(sys.argv[3])
    rows = load("claude_counts.csv") + load("claude_counts_extra.csv")
    for r in rows:
        for k in ("chars", "o200k", "claude_tokens", "baseline_before", "baseline_after"):
            r[k] = int(r[k])
    out = []

    # 1. Do the models share a tokenizer? Compare every model with Sonnet 5.5 file by file.
    by_file = defaultdict(dict)
    for r in rows:
        by_file[(r["repo"], r["path"])][r["model"]] = r
    models = sorted({r["model"] for r in rows})
    out.append("## Same file, different Claude models\n")
    out.append("| Model | Files | Total tokens | vs Sonnet 5.5 | Files that differ | Baseline before/after |")
    out.append("| --- | --- | --- | --- | --- | --- |")
    for m in models:
        rs = [r for r in rows if r["model"] == m]
        tot = sum(r["claude_tokens"] for r in rs)
        ref = sum(by_file[(r["repo"], r["path"])][NEW]["claude_tokens"] for r in rs)
        diff = sum(r["claude_tokens"] != by_file[(r["repo"], r["path"])][NEW]["claude_tokens"] for r in rs)
        bases = sorted({f"{r['baseline_before']}/{r['baseline_after']}" for r in rs})
        out.append(f"| {m} | {len(rs)} | {tot:,} | {tot / ref:.3f}x | {diff} | {', '.join(bases)} |")

    # 2. Ratios per file type, against o200k_base and between the two Claude tokenizers.
    ratios = {"claude": {}, "claude_legacy": {}}
    out.append("\n## Per file type (Sonnet 5.5 = new tokenizer, Sonnet 4.6 = previous one)\n")
    out.append("| Type | Files | Chars | o200k | Claude new | new/o200k | per-file range | Claude 4.6 | 4.6/o200k "
               "| new/4.6 | chars per new token | Qwen3/new | Xenova claude-tokenizer/new |")
    out.append("| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |")
    kinds = defaultdict(list)
    for key, ms in by_file.items():
        if NEW in ms and LEGACY in ms:
            kinds[ms[NEW]["kind"]].append(ms)
    sample_texts = {}

    def text_of(repo, path):
        if (repo, path) not in sample_texts:
            sample_texts[(repo, path)] = (repos_dir / repo / path).read_text(encoding="utf-8")
        return sample_texts[(repo, path)]

    code_kinds = {"python", "javascript", "typescript", "go", "rust"}
    groups = sorted(kinds.items(), key=lambda kv: (kv[0] not in code_kinds, kv[0]))
    groups.append(("all code", [ms for k, v in kinds.items() if k in code_kinds for ms in v]))
    groups.append(("all", [ms for v in kinds.values() for ms in v]))
    for kind, files in groups:
        chars = sum(f[NEW]["chars"] for f in files)
        o = sum(f[NEW]["o200k"] for f in files)
        new = sum(f[NEW]["claude_tokens"] for f in files)
        old = sum(f[LEGACY]["claude_tokens"] for f in files)
        q = sum(len(qwen.encode(text_of(f[NEW]["repo"], f[NEW]["path"]), add_special_tokens=False).ids)
                for f in files)
        x = sum(len(xenova.encode(text_of(f[NEW]["repo"], f[NEW]["path"]), add_special_tokens=False).ids)
                for f in files)
        per = [f[NEW]["claude_tokens"] / f[NEW]["o200k"] for f in files]
        out.append(f"| {kind} | {len(files)} | {chars:,} | {o:,} | {new:,} | {new / o:.2f} | "
                   f"{min(per):.2f} to {max(per):.2f} | {old:,} | {old / o:.2f} | {new / old:.2f} | "
                   f"{chars / new:.2f} | {q / new:.2f} | {x / new:.2f} |")
        if kind not in ("all code",):
            key = "other" if kind == "all" else kind
            ratios["claude"][key] = round(new / o, 3)
            ratios["claude_legacy"][key] = round(old / o, 3)

    (HERE / "claude_ratios.json").write_text(json.dumps({
        "measured": "2026-10-16 with claude -p (Claude Code 2.1.301), see results/claude_counts*.csv",
        "note": "Claude tokens per o200k_base token, by file type. 'claude' = Claude Opus 4.7 and later "
                "(measured on Sonnet 5.5; Haiku 5.5, Opus 5.5 and Opus 4.7 gave the same counts or close), "
                "'claude_legacy' = Sonnet 4.6 and earlier. 'other' is the ratio over all sampled files.",
        "ratio_to_o200k": ratios}, indent=2) + "\n")

    # 3. Whole-repo check on repos the ratios were not measured on.
    vpath = RES / "validation.json"
    if vpath.exists():
        out.append("\n## Whole repos the ratios were not measured on\n")
        out.append("| Repo | Model | Files | Measured | Estimate | Error | o200k as is | Error | chars/4 rule | Error |")
        out.append("| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |")
        for v in json.loads(vpath.read_text()):
            rs, _ = count_repo(repos_dir / v["repo"])
            tk = "claude" if v["model"] == NEW else "claude_legacy"
            r_ = ratios[tk]
            est = round(sum(r["o200k"] * r_.get(r["kind"], r_["other"]) for r in rs))
            o = sum(r["o200k"] for r in rs)
            c4 = sum(r["chars"] for r in rs) // 4
            m = v["claude_tokens"]
            out.append(f"| {v['repo']} | {v['model']} | {v['files']} | {m:,} | {est:,} | {100 * (est - m) / m:+.1f}% | "
                       f"{o:,} | {100 * (o - m) / m:+.1f}% | {c4:,} | {100 * (c4 - m) / m:+.1f}% |")

    (RES / "summary.md").write_text("# Results\n\nGenerated by analyze.py.\n\n" + "\n".join(out) + "\n")
    print("\n".join(out))


if __name__ == "__main__":
    main()
