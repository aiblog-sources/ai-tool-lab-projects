#!/usr/bin/env python3
"""Summary tables. Usage: python3 bench/summarize.py results/runs.jsonl results/combo-runs.jsonl > results/summary.md"""
import json, statistics as st, sys
from collections import defaultdict

allruns = [json.loads(l) for f in sys.argv[1:] for l in open(f)]
runs = [r for r in allruns if r["task"] != "combo"]
combo = [r for r in allruns if r["task"] == "combo"]
CONDS = ["none", "claude-md", "post-exit1", "post-exit2", "stop-exit2"]
g = defaultdict(list)
for r in runs: g[(r["model"], r["shell"], r["condition"])].append(r)
mean = lambda xs: st.mean(xs) if xs else 0
med = lambda xs: st.median(xs) if xs else 0

print(f"{len(runs)} runs. Models: {sorted({r['model_id'] for r in runs})}.\n")
for shell in ("shell", "noshell"):
    print(f"## {'Bash allowed (npx, npm, node, tsc, grep...)' if shell == 'shell' else 'No shell (file tools only)'}\n")
    print("| Model | Condition | Runs | Runs with tsc errors left | Errors left (total) | Hidden tests all pass | Claude tried to run tsc (median; denied without a shell) | Hook runs (median) | Hook reports with errors (median) | Hook time per run (median, s) | Turns (median) | Wall time (median, s) | Cost per run (mean, USD) |")
    print("| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |")
    for model in ("haiku", "sonnet"):
        for c in CONDS:
            rs = g.get((model, shell, c), [])
            if not rs: continue
            print(f"| {model} | {c} | {len(rs)} | {sum(r['tsc_errors_left'] > 0 for r in rs)} | {sum(r['tsc_errors_left'] for r in rs)} | "
                  f"{sum(r['tests_ok'] for r in rs)}/{len(rs)} | {med([r['claude_ran_tsc'] for r in rs]):g} | {med([r['hook_runs'] for r in rs]):g} | "
                  f"{med([r['hook_failures'] for r in rs]):g} | {med([r['hook_ms_total'] / 1000 for r in rs]):.1f} | {med([r['turns'] or 0 for r in rs]):g} | "
                  f"{med([r['wall_s'] for r in rs]):.0f} | {mean([r['cost_usd'] or 0 for r in rs]):.4f} |")
    print()

print("## Per task: runs with tsc errors left / runs with a hidden test failing\n")
tasks = list(dict.fromkeys(r["task"] for r in runs))
print("| Task | " + " | ".join(CONDS) + " |"); print("| --- |" + " --- |" * len(CONDS))
for t in tasks:
    cells = []
    for c in CONDS:
        rs = [r for r in runs if r["task"] == t and r["condition"] == c]
        cells.append(f"{sum(r['tsc_errors_left'] > 0 for r in rs)} / {sum(not r['tests_ok'] for r in rs)} of {len(rs)}")
    print(f"| {t} | " + " | ".join(cells) + " |")
print()

print("## Hook output as Claude Code recorded it (transcript attachment types, summed)\n")
att = defaultdict(lambda: defaultdict(int))
for r in runs:
    for k, v in (r.get("hook_attachments") or {}).items(): att[r["condition"]][k] += v
for c in CONDS:
    if att[c]: print(f"- {c}: " + ", ".join(f"`{k}` x{v}" for k, v in sorted(att[c].items())))
print()

print("## Runs that left tsc errors or failed a hidden test\n")
for r in runs:
    if r["tsc_errors_left"] or not r["tests_ok"]:
        print(f"- {r['task']} / {r['model']} / {r['shell']} / {r['condition']} / rep {r['rep']}: {r['tsc_errors_left']} tsc errors, "
              f"tests {r['tests_passed']}/{r['tests_passed'] + r['tests_failed']}, subtype {r['subtype']}"
              + (f"; first error: `{r['tsc_error_lines'][0][:160]}`" if r['tsc_error_lines'] else ""))

if combo:
  for shell in ("noshell", "shell"):
    print(f"\n## Combo task: all six changes in one prompt, {'no shell' if shell == 'noshell' else 'Bash allowed'} (graded on tsc only)\n")
    print("| Model | Condition | Runs | Runs with tsc errors left | Errors left per run | Hook runs (median) | Hook reports with errors (median) | Turns (median) | Wall time (median, s) | Cost per run (mean, USD) | Result subtypes |")
    print("| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |")
    for model in ("haiku", "sonnet"):
        for c in CONDS:
            rs = [r for r in combo if r["model"] == model and r["condition"] == c and r["shell"] == shell]
            if not rs: continue
            print(f"| {model} | {c} | {len(rs)} | {sum(r['tsc_errors_left'] > 0 for r in rs)} | {', '.join(str(r['tsc_errors_left']) for r in rs)} | "
                  f"{med([r['hook_runs'] for r in rs]):g} | {med([r['hook_failures'] for r in rs]):g} | {med([r['turns'] or 0 for r in rs]):g} | "
                  f"{med([r['wall_s'] for r in rs]):.0f} | {mean([r['cost_usd'] or 0 for r in rs]):.4f} | {', '.join(sorted({str(r['subtype']) for r in rs}))} |")
