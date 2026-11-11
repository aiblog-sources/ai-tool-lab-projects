"""Tables for the README and the post, from results/*/result.json and results/loop-*/check-calls.jsonl.
usage: summary.py > results/summary.txt"""
import json
from pathlib import Path

R = Path(__file__).resolve().parent.parent / "results"


def load(p):
    f = R / p / "result.json"
    return json.loads(f.read_text()) if f.exists() else None


base = load("fighters-baseline")
print(f"baseline fighters: {base['fps']} loops/s, slowest loop {base['max_loop_cycles']:,} cycles\n")
print("One-shot fixes (claude -p, no tools), fighters scene, game loops per second")
print("| Report | Round 1 | Round 2 |")
print("| --- | --- | --- |")
total = 0.0
for kind in ("none", "loops", "loopfn"):
    r1, r2 = [], []
    for rep in range(1, 6):
        a, b = load(f"fighters-{kind}-{rep}"), load(f"fighters-{kind}-{rep}-r2")
        if a:
            r1.append(f"{a.get('fps', 'no build')}")
            total += a.get("cost_usd", 0)
        if b and not b.get("skipped"):
            r2.append(f"{b.get('fps', 'no build')}")
            total += b.get("cost_usd", 0)
        elif b:
            r2.append("not needed")
    print(f"| {kind} | {', '.join(r1)} | {', '.join(r2)} |")
print(f"one-shot USD at API prices, both rounds: {total:.2f}\n")

print("Agent loop (Claude Code, Read/Edit/Write + Bash(./check.sh)), fighters scene")
print("| check.sh prints | Run | check.sh calls | Final loops/s | Slowest loop | Mean load | Minutes | USD |")
print("| --- | --- | --- | --- | --- | --- | --- | --- |")
ltotal = 0.0
for kind in ("rate", "loops", "loopfn"):
    for rep in range(1, 5):
        r = load(f"loop-{kind}-{rep}")
        if not r:
            continue
        ltotal += r.get("cost_usd") or 0
        print(f"| {kind} | {rep} | {r['check_calls']} | {r.get('fps')} | "
              f"{r.get('max_loop_cycles', 0):,} | {r.get('mean_load')}% | {r['claude_s'] / 60:.1f} | "
              f"{(r.get('cost_usd') or 0):.2f} |")
print(f"agent loop USD at API prices: {ltotal:.2f}")
