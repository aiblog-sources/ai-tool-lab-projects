"""One table from every ai-test/*/result.json (writes results/ai-test-summary.md and results/ai-test.json)."""
import json
from collections import defaultdict
from pathlib import Path

HERE = Path(__file__).resolve().parent.parent
rows = [json.loads(p.read_text()) for p in sorted((HERE / "ai-test").glob("*/result.json"))]
(HERE / "results" / "ai-test.json").write_text(json.dumps(rows, indent=1))
MODELS = ["claude-sonnet-5-5", "claude-opus-5-5", "claude-haiku-5-5"]
rows.sort(key=lambda r: (MODELS.index(r["model"]), r["rep"], r["task"]))


def prof(stage):
    p = stage.get("profile", {}) if stage else {}
    if not stage:
        return "-"
    if not stage.get("builds"):
        return "no build"
    return f"{p.get('mean_load', 0):.1f} / {p.get('peak_load', 0):.0f} / {p.get('dropped', '?')}"


def lintc(stage):
    if not stage or not stage.get("builds"):
        return "-"
    l = stage["lint"]
    return f"{l['hot']}/{l['frame']}/{l['guarded']}"


out = ["| Model | Task | Rep | Build fix rounds | v0 lint hot/frame/guarded | v0 load mean / peak / dropped | "
       "after lint v1 fix | after lint v2 fix | after plain fix | USD |", "|" + " --- |" * 10]
agg = defaultdict(lambda: defaultdict(float))
for r in rows:
    v0 = r.get("v0", {})
    out.append(f"| {r['model'].replace('claude-', '')} | {r['task']} | {r['rep']} | {v0.get('build_rounds', '-')} | "
               f"{lintc(v0)} | {prof(v0)} | {prof(r.get('lint'))} | {prof(r.get('lint2'))} | {prof(r.get('plain'))} | "
               f"{r['cost_usd']:.2f} |")
    a = agg[r["model"]]
    a["programs"] += 1
    a["built_first_try"] += v0.get("build_rounds") == 0 and v0.get("builds", False)
    a["built"] += v0.get("builds", False)
    a["usd"] += r["cost_usd"]
    a["calls"] += r["calls"]
    if v0.get("builds"):
        a["load_sum"] += v0["profile"]["mean_load"]
        a["dropped_programs"] += v0["profile"]["dropped"] > 0
        a["dropped_frames"] += v0["profile"]["dropped"]
        a["hot_frame"] += v0["lint"]["hot"] + v0["lint"]["frame"]
        a["guarded"] += v0["lint"]["guarded"]
        a["with_findings"] += bool(v0["findings"])
        for k in ("lint", "lint2", "plain"):
            s = r.get(k)
            if s and s.get("builds"):
                a[f"{k}_n"] += 1
                a[f"{k}_load_delta"] += s["profile"]["mean_load"] - v0["profile"]["mean_load"]
                a[f"{k}_dropped"] += s["profile"]["dropped"]
                a[f"{k}_guarded"] += s["lint"]["guarded"] + s["lint"]["hot"] + s["lint"]["frame"]
            elif s and "skipped" not in s:
                a[f"{k}_broke"] += 1
        l2 = v0.get("lint2", {})
        a["v2_heavy"] += l2.get("heavy", 0)
        a["v2_hot_frame"] += l2.get("hot", 0) + l2.get("frame", 0)
out.append("")
out.append("| Model | Programs | Built first try | Built | Mean load | Programs with drops | Dropped frames | "
           "Lint v1 hot+frame | Lint v1 guarded | Lint v2 heavy | Lint v2 hot+frame | Calls | USD |")
out.append("|" + " --- |" * 13)
for m in MODELS:
    a = agg.get(m)
    if not a:
        continue
    n = a["built"] or 1
    out.append(f"| {m.replace('claude-', '')} | {a['programs']:.0f} | {a['built_first_try']:.0f} | {a['built']:.0f} | "
               f"{a['load_sum'] / n:.1f}% | {a['dropped_programs']:.0f} | {a['dropped_frames']:.0f} | "
               f"{a['hot_frame']:.0f} | {a['guarded']:.0f} | {a['v2_heavy']:.0f} | {a['v2_hot_frame']:.0f} | "
               f"{a['calls']:.0f} | {a['usd']:.2f} |")
out.append("")
out.append("| Model | Fix | Fixed programs that built | Did not build | Mean load change (points) | Dropped frames after | "
           "Findings after |")
out.append("|" + " --- |" * 7)
for m in MODELS:
    a = agg.get(m)
    if not a:
        continue
    for k in ("lint", "lint2", "plain"):
        n = a[f"{k}_n"] or 1
        out.append(f"| {m.replace('claude-', '')} | {k} | {a[f'{k}_n']:.0f} | {a[f'{k}_broke']:.0f} | "
                   f"{a[f'{k}_load_delta'] / n:+.2f} | {a[f'{k}_dropped']:.0f} | {a[f'{k}_guarded']:.0f} |")
text = "\n".join(out) + "\n"
(HERE / "results" / "ai-test-summary.md").write_text(text)
print(text)
