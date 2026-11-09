"""Tables from results/*/result.json: one row per run, then one per scene x report kind x round, then the loop runs."""
import json
from collections import defaultdict
from pathlib import Path

R = Path(__file__).resolve().parent.parent / "results"
KINDS = ["none", "dma", "prof", "both", "loops"]
res = {p.parent.name: json.loads(p.read_text()) for p in sorted(R.glob("*/result.json"))}
runs = [r for r in res.values() if "kind" in r and not r.get("skipped")]

print(f"{'run':20} {'in fps':>6} {'builds':>6} {'rounds':>6} {'fps':>5} {'load':>5} {'p95':>5} {'max loop':>9} "
      f"{'queue':>6} {'spill':>5} {'USD':>7}")
for name in ("fighters-baseline", "title-baseline"):
    b = res[name]
    print(f"{name:20} {'':>6} {'yes':>6} {0:>6} {b['fps']:>5} {b['mean_load']:>5} {b['p95_load']:>5} "
          f"{b.get('max_loop_cycles', ''):>9} {b['max_queued']:>6} {b['spill_frames']:>5}")
order = lambda r: (r["scene"], r.get("round", 1), KINDS.index(r["kind"]), r["rep"])
for r in sorted(runs, key=order):
    print(f"{r['id']:20} {r.get('input_fps', ''):>6} {('yes' if r.get('builds') else 'NO'):>6} "
          f"{r.get('build_rounds', '-'):>6} {r.get('fps', '-'):>5} {r.get('mean_load', '-'):>5} {r.get('p95_load', '-'):>5} "
          f"{r.get('max_loop_cycles', '-'):>9} {r.get('max_queued', '-'):>6} {r.get('spill_frames', '-'):>5} {r['cost_usd']:>7}")

print("\nper scene, round and report kind (steady game loops per second, frames 700 to 1,499)")
g = defaultdict(list)
for r in runs:
    g[(r["scene"], r.get("round", 1), r["kind"])].append(r)
for (s, rnd, k), rs in sorted(g.items(), key=lambda x: (x[0][0], x[0][1], KINDS.index(x[0][2]))):
    fps = [r.get("fps") for r in sorted(rs, key=lambda r: r["rep"])]
    at60 = sum(1 for f in fps if f == 60.0)
    print(f"{s:9} round {rnd} {k:5} fps {fps}  at 60: {at60}/{len(rs)}  USD {sum(r['cost_usd'] for r in rs):.2f}")
tot = sum(r["cost_usd"] for r in runs)
print(f"\nfix runs {len(runs)}, claude -p calls {sum(r['calls'] for r in runs)}, USD {tot:.2f} at API prices, "
      f"output tokens {sum(r['output_tokens'] for r in runs):,}, Claude time {sum(r['claude_s'] for r in runs) / 60:.0f} min")

loops = [v for k, v in res.items() if k.startswith("loop-")]
if loops:
    print("\nagent loop (Claude Code with ./check.sh, Sonnet 5.5)")
    for r in sorted(loops, key=lambda r: r["id"]):
        print(f"{r['id']:18} builds {r.get('builds')} fps {r.get('fps')} load {r.get('mean_load')} "
              f"max loop {r.get('max_loop_cycles')} check.sh calls {r['check_calls']} turns {r.get('turns')} "
              f"USD {r.get('cost_usd')} ({r.get('subtype')}) {r['claude_s'] / 60:.0f} min")
    print(f"loop USD {sum(r.get('cost_usd') or 0 for r in loops):.2f}")
