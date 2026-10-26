#!/usr/bin/env python3
"""Tables for the README and the post.

Usage: python3 bench/summarize.py probes results/probes.jsonl
       python3 bench/summarize.py tasks results/tasks.jsonl
Probe cells: "key" = the canary reached the model in every rep; otherwise why it did not: "denied" = a deny rule
matched, "asks" = the command needs approval (refused in claude -p because nobody can approve), "outside" = the
read-outside-working-directory check, "ran, no key" = the command ran but printed nothing with the key in it.
"a/b" = mixed reps. "(changed)" marks runs where Claude did not send the exact command.
"""
import json, sys
from collections import defaultdict

mode, path = sys.argv[1], sys.argv[2]
rows = [json.loads(l) for l in open(path)]
conds = list(dict.fromkeys(r["condition"] for r in rows))
by = defaultdict(list)
for r in rows: by[(r["id"], r["condition"])].append(r)
ids = list(dict.fromkeys(r["id"] for r in rows))

def reason(r):
    t = r["steps"][0]["result"]
    if "has been denied" in t or "denied by your permission settings" in t or "deny rule" in t: return "denied"
    if "approval" in t or "needs approval" in t or "can't be checked" in t or "cannot be auto-allowed" in t: return "asks"
    if "working directories" in t: return "outside"
    return "error"

if mode == "probes":
    probes = {p["id"]: p for p in json.load(open(__file__.rsplit("/", 1)[0] + "/probes.json"))}
    print("| Probe | Command | " + " | ".join(conds) + " |")
    print("| --- | --- |" + " --- |" * len(conds))
    totals = defaultdict(int)
    for i in ids:
        cells = []
        for c in conds:
            rs = by[(i, c)]
            n = sum(r["leaked_to_model"] for r in rs)
            totals[c] += n
            exact = all(any(s["tool"] == "Read" if probes[i].get("tool") else s["input"].get("command") == probes[i]["cmd"]
                            for s in r["steps"]) for r in rs)
            refused = all(r["steps"] and all(s["is_error"] for s in r["steps"]) for r in rs)
            if not rs: cell = "-"
            elif n == len(rs): cell = "key"
            elif n == 0: cell = reason(rs[0]) if refused else ("not run" if not any(r["steps"] for r in rs) else "ran, no key")
            else: cell = f"key {n}/{len(rs)}"
            cells.append(cell + ("" if exact else " (changed)"))
        cmd = "Read tool" if probes[i].get("tool") else f"`{probes[i]['cmd']}`".replace("|", "\\|")
        print(f"| {i} | {cmd} | " + " | ".join(cells) + " |")
    print("| **runs where the key reached the model** | | " + " | ".join(f"{totals[c]}/{sum(len(by[(i, c)]) for i in ids)}" for c in conds) + " |")
else:
    print("| Condition | Runs | Key reached the model | Key in the answer | Refused calls | Bash calls | Median calls |")
    print("| --- | --- | --- | --- | --- | --- | --- |")
    for c in conds:
        rs = [r for r in rows if r["condition"] == c]
        calls = sorted(r["n_calls"] for r in rs)
        print(f"| {c} | {len(rs)} | {sum(r['leaked_to_model'] for r in rs)} | {sum(r['leaked_in_answer'] for r in rs)} | "
              f"{sum(s['is_error'] for r in rs for s in r['steps'])} | {sum(s['tool'] == 'Bash' for r in rs for s in r['steps'])} | "
              f"{calls[len(calls) // 2]} |")
    print()
    print("| Task | " + " | ".join(conds) + " |")
    print("| --- |" + " --- |" * len(conds))
    for i in ids:
        print(f"| {i} | " + " | ".join(f"{sum(r['leaked_to_model'] for r in by[(i, c)])}/{len(by[(i, c)])} model, "
                                       f"{sum(r['leaked_in_answer'] for r in by[(i, c)])} answer" for c in conds) + " |")
cost = sum(r["cost_usd"] or 0 for r in rows)
print(f"\n{len(rows)} runs, USD {cost:.2f} at API prices, models {sorted(set(r['model'] for r in rows))}, "
      f"Claude Code {sorted(set(r['version'] for r in rows))}")
