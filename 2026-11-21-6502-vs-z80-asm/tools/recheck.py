#!/usr/bin/env python3
"""Re-checks the saved routines of one task with the current checker (no new
Claude calls) and rewrites final.json: the first round that passes becomes
the last one. Rounds after it, which answered a report from the old checker,
move to <dir>/superseded/.

  recheck.py TASK
"""
import json
import shutil
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(HERE / "tools"))
import asmrun  # noqa: E402

name = sys.argv[1]
for fin in sorted((HERE / "ai-test").glob(f"*/{name}-*/final.json")):
    d = fin.parent
    f = json.loads(fin.read_text())
    old = json.loads(json.dumps(f))
    kept = []
    for r in f["rounds"]:
        src = d / f"round{r['round']}.{'s' if f['cpu'] == '6502' else 'asm'}"
        res = asmrun.run(f["cpu"], name, src, asmrun.WORK / "runs" / f"recheck-{d.parent.name}-{d.name}-{r['round']}")
        (d / f"round{r['round']}-result.json").write_text(json.dumps(res, indent=1, default=str))
        r.update(assembled=res["assembled"], passed=res.get("passed"), vectors=res.get("vectors"), ok=res.get("ok", False),
                 fail_kinds=res.get("fail_kinds"), cycles_max=res.get("cycles_max"), cycles_median=res.get("cycles_median"))
        kept.append(r)
        if r["ok"]:
            break
    later = [r for r in f["rounds"] if r["round"] > kept[-1]["round"]]
    if later:
        (d / "superseded").mkdir(exist_ok=True)
        for r in later:
            for p in d.glob(f"round{r['round']}*"):
                shutil.move(str(p), d / "superseded" / p.name)
        (d / "superseded" / "final-old-checker.json").write_text(json.dumps(old, indent=1))
    f.update(rounds=kept, ok=kept[-1]["ok"], first_try=kept[0]["ok"], fix_rounds=len(kept) - 1)
    fin.write_text(json.dumps(f, indent=1))
    print(d.parent.name, d.name, "first try", f["first_try"], "ok", f["ok"], "rounds", len(kept),
          [(r["passed"], r["vectors"]) for r in kept])
