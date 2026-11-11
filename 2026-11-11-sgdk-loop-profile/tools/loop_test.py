"""The agent loop: Claude Code (Sonnet 5.5) gets the fighters scene in a work folder with ./check.sh, which builds
it with SGDK 2.11 and prints one kind of report (--kind rate, loops or loopfn; see loop_check.py). It may read, edit and run
check.sh as often as it likes, within a USD budget. The final main.c is then measured by this script, not taken
from Claude's word. Work folders are under /tmp (no CLAUDE.md from a parent folder); results in results/loop-<n>/.
usage: loop_test.py --kind rate|loops|loopfn [--reps 4] [--budget 4]"""
import argparse
import json
import shutil
import subprocess
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import mdperf  # noqa: E402
from loop_check import FRAMES, FROM, INPUTS  # noqa: E402
from sgdk_build import WORK, build  # noqa: E402

HERE = Path(__file__).resolve().parent.parent
TMP = Path("/tmp/mdperf-loop")
ENV = dict(__import__("os").environ, CLAUDE_CODE_DISABLE_CLAUDE_MDS="1", CLAUDE_CODE_DISABLE_AUTO_MEMORY="1")
WHAT = {
    "rate": "and prints how many game loops per second it ran",
    "loops": "under two checkers: one logs every DMA transfer, the other charges every 68000 cycle to a function "
             "and adds up the cost of each game loop",
    "loopfn": "under two checkers: one logs every DMA transfer, the other charges every 68000 cycle to a function "
              "and adds up the cost of each game loop, by function, over every video frame the loop took",
}
PROMPT = """src/main.c is a Mega Drive scene written with SGDK 2.11. It must run at a steady 60 frames per second on \
a real NTSC console, and it does not. ./check.sh builds src/main.c and runs it for 1,500 frames on an emulated NTSC \
console with a scripted pad, {what}. Run it as often as you like. Fix the slowdown without \
changing what the scene shows: same objects, same sizes, same animations and behavior. Stop when check.sh reports \
60 game loops per second, or when you cannot get further, and say which."""


def run(kind, rep, model, budget):
    name = f"fighters-loop-{kind}-{rep}"
    dest = HERE / "results" / f"loop-{kind}-{rep}"
    if (dest / "result.json").exists():
        return json.loads((dest / "result.json").read_text())
    wd = TMP / name
    if wd.exists():
        shutil.rmtree(wd)
    (wd / "src").mkdir(parents=True)
    shutil.copy(HERE / "scenes" / "fighters.c", wd / "src" / "main.c")
    (wd / "check.sh").write_text(f'#!/bin/sh\nexec python3 {Path(__file__).resolve().parent / "loop_check.py"} '
                                 f'"$(cd "$(dirname "$0")" && pwd)" {kind}\n')
    (wd / "check.sh").chmod(0o755)
    log = WORK / "loop-calls" / f"{name}.jsonl"
    if log.exists():
        log.unlink()
    dest.mkdir(parents=True, exist_ok=True)
    t0 = time.time()
    p = subprocess.run(["claude", "-p", "--model", model, "--allowedTools", "Read", "Edit", "Write", "Bash(./check.sh)",
                        "--strict-mcp-config", "--setting-sources", "", "--max-budget-usd", str(budget),
                        "--output-format", "json"], input=PROMPT.format(what=WHAT[kind]), capture_output=True, text=True, env=ENV, cwd=wd,
                       timeout=5400)
    wall = round(time.time() - t0, 1)
    try:
        d = json.loads(p.stdout)
    except json.JSONDecodeError:
        d = dict(result=p.stdout[-2000:] + p.stderr[-2000:], is_error=True)
    (dest / "reply.md").write_text(str(d.get("result", "")))
    calls = [json.loads(l) for l in open(log)] if log.exists() else []
    (dest / "check-calls.jsonl").write_text("".join(json.dumps(c) + "\n" for c in calls))
    code = (wd / "src" / "main.c").read_text()
    (dest / "main.c").write_text(code)
    ok, errs, bd = build(code, f"{name}-final")
    res = dict(id=name, kind=kind, model=model, budget_usd=budget, builds=ok, cost_usd=d.get("total_cost_usd"),
               turns=d.get("num_turns"), subtype=d.get("subtype"), is_error=d.get("is_error"), claude_s=wall,
               check_calls=len(calls), claude_code_version=d.get("version"))
    if ok:
        s, _ = mdperf.analyze(bd, f"{name}-final", FRAMES, FROM, INPUTS, 1000, dest)
        res.update(s)
    (dest / "result.json").write_text(json.dumps(res, indent=1))
    print(time.strftime("%H:%M:%S"), name, {k: res.get(k) for k in ("builds", "fps", "mean_load", "max_loop_cycles",
          "check_calls", "turns", "cost_usd", "subtype")}, flush=True)
    return res


ap = argparse.ArgumentParser()
ap.add_argument("--kind", required=True, choices=list(WHAT))
ap.add_argument("--reps", type=int, default=4)
ap.add_argument("--budget", type=float, default=4.0)
ap.add_argument("--model", default="claude-sonnet-5-5")
a = ap.parse_args()
from concurrent.futures import ThreadPoolExecutor  # noqa: E402
with ThreadPoolExecutor(3) as ex:
    rs = list(ex.map(lambda r: run(a.kind, r, a.model, a.budget), range(1, a.reps + 1)))
print("total USD", round(sum(r.get("cost_usd") or 0 for r in rs), 2))
