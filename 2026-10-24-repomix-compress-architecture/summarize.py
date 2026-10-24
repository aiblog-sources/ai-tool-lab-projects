#!/usr/bin/env python3
"""Join the hand grades with the runs and print the result tables (Markdown).

Grades are in results/grades.psv (label|points|wrong_claims|note), written blind: the grader saw
results/grading-sheet.md, where answers are shuffled and labelled, and results/grading-key.json
maps labels back to runs. points is one digit (0 or 1) per checklist point in questions.json.
usage: summarize.py results/
"""
import collections
import json
import re
import statistics
import sys
from pathlib import Path

res = Path(sys.argv[1])
spec = json.load(open("questions.json"))
runs = {(r["id"], r["condition"], r["rep"]): r for r in map(json.loads, open(res / "runs.jsonl"))}
key = json.load(open(res / "grading-key.json"))
grades = {}
for line in list(open(res / "grades.psv"))[1:]:
    label, pts, wrong, note = line.rstrip("\n").split("|")
    k = key[label]
    grades[(k["id"], k["condition"], k["rep"])] = ([int(c) for c in pts], int(wrong), note)
assert set(grades) == set(runs), "every run needs a grade"
repo_of = {q["id"]: q["repo"] for q in spec["questions"]}
CONDS = ["none", "full", "compress", "compress-patched"]
LABEL = {"none": "No repo (model memory)", "full": "Full pack", "compress": "`--compress`",
         "compress-patched": "`--compress`, private methods fixed"}
# An answer that says part of the answer is not in the pack.
GAP = re.compile(r"not shown|missing|compressed|stripped|can't confirm|cannot confirm|isn't visible|"
                 r"not visible|elided|aren't shown|cut off|doesn't show|does not show", re.I)


def cell(rows):
    pts = sum(sum(g[0]) for g in rows)
    return f"{pts}/{5 * len(rows)}"


print("## Checklist points by repo and condition (5 questions x 5 points x 2 runs)\n")
print("| Condition | ky (TypeScript) | requests (Python) | diff-tour (Go, new) | All | Wrong claims |")
print("| --- | --- | --- | --- | --- | --- |")
for c in CONDS:
    out, allrows = [], []
    for repo in spec["repos"]:
        rows = [g for (i, cc, _), g in grades.items() if cc == c and repo_of[i] == repo]
        out.append(cell(rows) if rows else "")
        allrows += rows
    print(f"| {LABEL[c]} | " + " | ".join(out) + f" | {cell(allrows)} | {sum(g[1] for g in allrows)} |")

print("\n## Per question (points out of 10: 5 points x 2 runs)\n")
print("| Question | " + " | ".join(LABEL[c] for c in CONDS) + " |")
print("| --- | " + " | ".join("---" for _ in CONDS) + " |")
for q in spec["questions"]:
    vals = []
    for c in CONDS:
        rows = [g for (i, cc, _), g in grades.items() if i == q["id"] and cc == c]
        vals.append(str(sum(sum(g[0]) for g in rows)) if rows else "")
    print(f"| {q['id']} | " + " | ".join(vals) + " |")

print("\n## Answers that say part of the answer is missing, \"NOT SURE\", tokens, time and cost\n")
print("| Condition | Runs | Says something is missing | NOT SURE | Median input tokens (ky / requests / diff-tour) | Median wall time | Cost at API prices |")
print("| --- | --- | --- | --- | --- | --- | --- |")
for c in CONDS:
    rr = [r for (i, cc, _), r in runs.items() if cc == c]
    gaps = sum(bool(GAP.search(r["answer"])) for r in rr if r["answer"].strip() != "NOT SURE")
    ns = sum(r["answer"].strip() == "NOT SURE" for r in rr)
    toks = []
    for repo in spec["repos"]:
        t = [r["input_tokens"] for r in rr if r["repo"] == repo]
        toks.append(f"{statistics.median(t):,.0f}" if t else "-")
    print(f"| {LABEL[c]} | {len(rr)} | {gaps if c != 'none' else '-'} | {ns} | {' / '.join(toks)} | "
          f"{statistics.median(r['wall_s'] for r in rr):.1f} s | ${sum(r['cost_usd'] for r in rr):.2f} |")

allr = list(runs.values())
print(f"\nTotal: {len(allr)} runs, ${sum(r['cost_usd'] for r in allr):.2f} at API prices, "
      f"model {allr[0]['model']}, Claude Code {sorted({r['claude_code'] for r in allr})}, "
      f"first run {min(r['ts'] for r in allr)}, last {max(r['ts'] for r in allr)}.")

print("\n## Wrong claims, by hand\n")
for (i, c, rep), g in sorted(grades.items()):
    if g[1]:
        print(f"- {i}, {LABEL[c]}, run {rep}: {g[1]} ({g[2]})")
