#!/usr/bin/env python3
"""Score the broad (list) answers against results/truth-broad.json and print a table per condition.

An answer line's first token (before a space or "(") is the item. Found = items in the truth list; extra = items
not in it (checked by hand afterwards: see results/extras.md).
usage: score_broad.py results/runs-broad.jsonl results/truth-broad.json
"""
import json
import re
import statistics as st
import sys
from collections import defaultdict

runs = [json.loads(l) for l in open(sys.argv[1])]
truth = {k: set(v) for k, v in json.load(open(sys.argv[2])).items()}
CONDS = ["no-subagent", "default", "explore", "explore-haiku"]


def items(answer):
    out = set()
    for line in answer.splitlines():
        line = line.strip().lstrip("-*0123456789. ").strip("`* ")
        m = re.match(r"([A-Za-z_][\w]*)", line)
        if m:
            out.add(m.group(1))
    return out


rows = defaultdict(list)
extras = defaultdict(set)
for r in runs:
    got = items(r["answer"])
    t = truth[r["id"]]
    found, extra = len(got & t), got - t
    extras[r["id"]] |= {(x, r["condition"]) for x in extra}
    rows[(r["id"], r["condition"])].append((found, len(t), len(extra), r))
print("| Question | Condition | Found (per run) | Extras (per run) | Cost (median) | Main context at end (median) | Wall s (median) | Explore used |")
print("|---|---|---|---|---|---|---|---|")
for q in truth:
    for c in CONDS:
        xs = rows.get((q, c), [])
        if not xs:
            continue
        rs = [x[3] for x in xs]
        print(f"| {q} | {c} | {', '.join(f'{x[0]}/{x[1]}' for x in xs)} | {', '.join(str(x[2]) for x in xs)} | "
              f"${st.median(r['cost_usd'] for r in rs):.3f} | {st.median(r['main_ctx_final'] for r in rs):,.0f} | "
              f"{st.median(r['wall_s'] for r in rs):.0f} | {sum(1 for r in rs if r['agent_calls'])}/{len(rs)} |")
print()
for c in CONDS:
    xs = [x for (q, cc), v in rows.items() if cc == c for x in v]
    if xs:
        rs = [x[3] for x in xs]
        print(f"{c}: found {sum(x[0] for x in xs)}/{sum(x[1] for x in xs)}, extras {sum(x[2] for x in xs)}, "
              f"cost ${sum(r['cost_usd'] for r in rs):.2f} total, median ctx {st.median(r['main_ctx_final'] for r in rs):,.0f}, "
              f"median main input all turns {st.median(r['main_input_total'] for r in rs):,.0f}, "
              f"median sub input {st.median(r['sub_input_total'] for r in rs):,.0f}, median wall {st.median(r['wall_s'] for r in rs):.0f}s, "
              f"haiku ${sum(r['cost_by_model'].get('claude-haiku-5-5', 0) for r in rs):.2f}")
print()
for q, ex in extras.items():
    print(q, "extras:", sorted(ex))
