"""Round-one runs were measured before mdperf had the game loop section. Emulation is deterministic, so re-run
mdperf on each run's built ROM and add max_loop_cycles (plus report-loops.txt) to its result."""
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import mdperf  # noqa: E402
from loop_check import FRAMES, FROM, INPUTS  # noqa: E402
from sgdk_build import WORK  # noqa: E402

R = Path(__file__).resolve().parent.parent / "results"
for p in sorted(R.glob("*/result.json")):
    r = json.loads(p.read_text())
    if "kind" not in r or "max_loop_cycles" in r or not r.get("builds"):
        continue
    proj = WORK / f"{r['id']}-{r['build_rounds']}"
    s, texts = mdperf.analyze(proj, proj.name, FRAMES, FROM, INPUTS)
    assert s["fps"] == r["fps"] and s["mean_load"] == r["mean_load"], (r["id"], s, r)
    r["max_loop_cycles"] = s["max_loop_cycles"]
    (p.parent / "report-loops.txt").write_text(texts["loops"])
    p.write_text(json.dumps(r, indent=1))
    print(r["id"], s["fps"], s["max_loop_cycles"])
