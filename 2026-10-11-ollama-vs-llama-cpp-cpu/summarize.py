#!/usr/bin/env python3
"""Turns results/runs.jsonl into a Markdown table: median (min-max) per model, config and engine."""
import json
import os
import statistics
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
path = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, "results", "runs.jsonl")
rows = [json.loads(line) for line in open(path) if line.strip()]
rows = [r for r in rows if not r["warmup"]]


def fmt(values, digits):
    return (f"{statistics.median(values):.{digits}f} "
            f"({min(values):.{digits}f}-{max(values):.{digits}f})")


print("| Model | Config | Engine | Runs | Prompt t/s | Generation t/s | Wall s |")
print("| --- | --- | --- | --- | --- | --- | --- |")
keys = []
for r in rows:
    k = (r["model"], r["label"], r["engine"])
    if k not in keys:
        keys.append(k)
for model, label, engine in keys:
    g = [r for r in rows if (r["model"], r["label"], r["engine"]) == (model, label, engine)]
    print(f"| {model} | {label} | {engine} | {len(g)} | {fmt([r['prompt_tps'] for r in g], 1)} | "
          f"{fmt([r['gen_tps'] for r in g], 2)} | {fmt([r['wall_s'] for r in g], 1)} |")
