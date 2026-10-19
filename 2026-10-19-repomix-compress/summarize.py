#!/usr/bin/env python3
"""Grade the answers and print the result tables.

An answer is "abstained" when it says NOT IN CONTEXT / NOT SURE, "right" when it matches the
question's pattern, otherwise "wrong". Hand corrections live in overrides.json as
{"<id>|<condition>|<rep>": "right" | "wrong" | "abstained"} with a reason.

usage: summarize.py runs.jsonl [results/evidence.csv] > results/summary.md
"""
import csv
import json
import re
import sys
from collections import defaultdict
from pathlib import Path

HERE = Path(__file__).parent
CONDS = ["none", "full", "nocomments", "compress"]
qs = {q["id"]: q for q in json.load(open(HERE / "questions.json"))["questions"]}
overrides = json.load(open(HERE / "overrides.json")) if (HERE / "overrides.json").exists() else {}
runs = [json.loads(line) for line in open(sys.argv[1])]
args = [a for a in sys.argv[1:] if not a.startswith("--")]
ev_path = args[1] if len(args) > 1 else HERE / "results/evidence.csv"
survives = {r["id"]: r["compress"] == "1" for r in csv.DictReader(open(ev_path))}


def grade(r):
    key = f"{r['id']}|{r['condition']}|{r['rep']}"
    if key in overrides:
        return overrides[key]["grade"]
    a = r["answer"]
    if re.search(r"NOT IN CONTEXT|NOT SURE", a):
        return "abstained"
    return "right" if re.search(qs[r["id"]]["pattern"], a) else "wrong"


for r in runs:
    r["grade"] = grade(r)

if "--dump" in sys.argv:
    for r in sorted(runs, key=lambda r: (r["id"], CONDS.index(r["condition"]), r["rep"])):
        print(f"{r['grade']:9} {r['id']}|{r['condition']}|{r['rep']}  {r['answer'][:160]!r}")
    sys.exit()


def table(title, rows_filter):
    print(f"\n### {title}\n")
    print("| Condition | Right | Wrong | Abstained | Runs |")
    print("| --- | --- | --- | --- | --- |")
    for c in CONDS:
        g = [r["grade"] for r in runs if r["condition"] == c and rows_filter(r)]
        if g:
            print(f"| {c} | {g.count('right')} | {g.count('wrong')} | {g.count('abstained')} | {len(g)} |")


n_surv = sum(survives.values())
print(f"# Results ({len(runs)} runs, model {runs[0]['model']}; versions in results/env.md)")
table("All 30 questions", lambda r: True)
table(f"The {n_surv} questions whose answer text survives --compress", lambda r: survives[r["id"]])
table(f"The {30 - n_surv} questions whose answer text --compress removes", lambda r: not survives[r["id"]])
for repo in ["ky", "requests", "cobra"]:
    table(f"{repo} only", lambda r, repo=repo: r["repo"] == repo)

print("\n### Input tokens per run (Claude's count, median over the repo's runs)\n")
print("| Repo | none | full | nocomments | compress | compress vs full |")
print("| --- | --- | --- | --- | --- | --- |")
for repo in ["ky", "requests", "cobra"]:
    med = {}
    for c in CONDS:
        v = sorted(r["input_tokens"] for r in runs if r["repo"] == repo and r["condition"] == c)
        med[c] = v[len(v) // 2] if v else 0
    print(f"| {repo} | {med['none']:,} | {med['full']:,} | {med['nocomments']:,} | {med['compress']:,} | "
          f"{med['compress'] / med['full']:.2f} |")

print("\n### Cost reported by claude -p (API prices; the runs used a subscription)\n")
print("| Condition | Total USD | Cache read share of input |")
print("| --- | --- | --- |")
for c in CONDS:
    rs = [r for r in runs if r["condition"] == c]
    tot = sum(r["input_tokens"] for r in rs) or 1
    print(f"| {c} | {sum(r['cost_usd'] or 0 for r in rs):.2f} | {sum(r['cache_read'] for r in rs) / tot:.0%} |")
print(f"\nTotal: USD {sum(r['cost_usd'] or 0 for r in runs):.2f}")

print("\n### Per question (right out of 2 runs)\n")
print("| Question | Answer text survives --compress | none | full | nocomments | compress |")
print("| --- | --- | --- | --- | --- | --- |")
by = defaultdict(list)
for r in runs:
    by[(r["id"], r["condition"])].append(r["grade"])
for qid in qs:
    cells = []
    for c in CONDS:
        g = by[(qid, c)]
        cells.append(f"{g.count('right')}" + (f" ({g.count('abstained')} abst.)" if "abstained" in g else ""))
    print(f"| {qid} | {'yes' if survives[qid] else 'no'} | " + " | ".join(cells) + " |")
