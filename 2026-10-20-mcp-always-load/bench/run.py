#!/usr/bin/env python3
"""Ask Claude Code (claude -p) each question under each loading condition and record what it did.

Usage: python3 bench/run.py <corpus-folder> <work-folder> <out.jsonl> [model]
The work folder should be empty: it is Claude Code's cwd, so built-in file tools find nothing there.
"""
import json, os, re, sys
from pathlib import Path
from claude_run import claude_retry

here = Path(__file__).resolve().parent
corpus, work, out = (Path(a).resolve() for a in sys.argv[1:4])
model = sys.argv[4] if len(sys.argv) > 4 else "sonnet"
questions = json.loads((here / "questions.json").read_text())
conditions = json.loads((here / "conditions.json").read_text())
only = os.environ.get("CONDITIONS")
if only:
    conditions = {k: v for k, v in conditions.items() if k in only.split(",")}
phrasings = {"hinted": "Check my notes. {q}", "bare": "{q}"}
done = set()
if out.exists():
    for line in out.read_text().splitlines():
        r = json.loads(line); done.add((r["qid"], r["phrasing"], r["condition"]))

with out.open("a") as f:
    for q in questions:              # conditions interleaved per question, so load and drift hit them alike
        for phrasing in phrasings:
            for cname, c in conditions.items():
                if (q["id"], phrasing, cname) in done: continue
                prompt = phrasings[phrasing].format(q=q["q"]) + " Answer in one or two sentences."
                r = {"qid": q["id"], "phrasing": phrasing, **claude_retry(prompt, cname, c, corpus, work, model)}
                r["correct"] = all(re.search(e, r["answer"], re.I) for e in q["expect"])
                f.write(json.dumps(r) + "\n"); f.flush()
                print(f"q{q['id']:>2} {phrasing:6} {cname:12} mcp={r['used_mcp']!s:5} search1st={r['searched_first']!s:5} "
                      f"ok={r['correct']!s:5} builtin={r['builtin']} ${r['cost_usd']} {r['wall_s']}s", flush=True)
