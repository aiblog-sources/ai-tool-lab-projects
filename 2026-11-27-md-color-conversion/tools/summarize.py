#!/usr/bin/env python3
"""Corpus summary: per method and display model, median of per-image mean CIEDE2000, worst pixel, images where the
method had the lowest mean error, lightness shift, and colors merged (images under 60 colors only, where a merge
is a choice rather than unavoidable). Reads results/corpus.json, writes results/corpus-summary.md. MIT license."""
import collections, json, statistics as st
from pathlib import Path

HERE = Path(__file__).resolve().parent.parent
rows = json.load(open(HERE / "results/corpus.json"))
lines = ["| Viewed on | Method | Median mean dE00 | Median p95 | Worst | Mean L* shift | Lowest error on | Merged (images < 60 colors) |",
         "| --- | --- | --- | --- | --- | --- | --- | --- |"]
for view in ("linear", "table"):
    sub = [r for r in rows if r["viewed_on"] == view]
    files = sorted({r["file"] for r in sub})
    best = collections.Counter()
    for f in files:
        rr = [r for r in sub if r["file"] == f]
        best[min(rr, key=lambda r: r["mean_de"])["method"]] += 1
    for meth in ["trunc", "macro", "round", "round-table", "lab", "distinct"]:
        mm = [r for r in sub if r["method"] == meth]
        small = [r for r in mm if r["source_colors"] < 60]
        lines.append(f"| {view} | {meth} | {st.median(r['mean_de'] for r in mm):.2f} | {st.median(r['p95_de'] for r in mm):.2f} | "
                     f"{max(r['max_de'] for r in mm):.1f} | {st.mean(r['mean_dL'] for r in mm):+.2f} | {best[meth]} of {len(files)} | "
                     f"{sum(r['merged'] for r in small)} of {sum(r['source_colors'] for r in small)} |")
(HERE / "results/corpus-summary.md").write_text("\n".join(lines) + "\n")
print("\n".join(lines))
