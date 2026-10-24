#!/usr/bin/env python3
"""Write the answers for hand grading with the condition hidden.

Answers are grouped by question and shuffled (fixed seed); each gets a short random label. The
label -> run mapping goes to a separate file that the grader does not open until grading is done.
usage: blind.py RUNS.jsonl SHEET.md KEY.json
"""
import json
import random
import sys

runs = [json.loads(l) for l in open(sys.argv[1])]
spec = json.load(open("questions.json"))
rng = random.Random(20261024)
key, out = {}, []
for q in spec["questions"]:
    rows = [r for r in runs if r["id"] == q["id"]]
    rng.shuffle(rows)
    out.append(f"## {q['id']}\n\n{q['q']}\n\nPoints:\n" +
               "".join(f"{i + 1}. {p}\n" for i, p in enumerate(q["points"])))
    for r in rows:
        label = f"{q['id']}/{rng.randrange(16**4):04x}"
        key[label] = {"id": r["id"], "condition": r["condition"], "rep": r["rep"]}
        out.append(f"### {label}\n\n{r['answer']}\n")
open(sys.argv[2], "w").write("\n".join(out))
json.dump(key, open(sys.argv[3], "w"), indent=1)
print(f"{len(key)} answers")
