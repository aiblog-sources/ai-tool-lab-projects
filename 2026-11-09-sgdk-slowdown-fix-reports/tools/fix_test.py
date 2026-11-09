"""Which report helps Claude fix slowdown in an SGDK scene: none, the DMA report, the CPU profile, or both?

The two scenes are the ones from the 2026-11-07 test that dropped frames on an emulated NTSC Mega Drive (Claude's own
code, written with "must run at a steady 60 fps on a real NTSC console"): `fighters` (36 game loops/s) and `title`
(32). For each scene x report kind x repetition:
  1. send the unchanged scene to `claude -p` (tools off, no CLAUDE.md, no settings) with one kind of report
  2. build the reply with SGDK 2.11; on compile errors send them back (up to 3 rounds)
  3. run it through mdperf (mddma + mdprof, same scripted pad, 1,500 frames, steady play = frames 700 to 1,499)
Everything lands in results/<scene>-<kind>-<rep>/. usage: fix_test.py [--model claude-sonnet-5-5] [--reps 3]
"""
import argparse
import json
import os
import re
import shutil
import subprocess
import threading
import time
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

import mdperf
from sgdk_build import WORK, build

HERE = Path(__file__).resolve().parent.parent
OUT = HERE / "results"
ENV = dict(os.environ, CLAUDE_CODE_DISABLE_CLAUDE_MDS="1", CLAUDE_CODE_DISABLE_AUTO_MEMORY="1")
SYSTEM = "You write C code for the Sega Mega Drive / Genesis using SGDK."
lock = threading.Lock()
FRAMES, FROM, SHOT = 1500, 700, 1000
# the pad script of the 2026-11-07 test
INPUTS = ["70-74:S", "100-400:R", "400-700:L", "700-800:U", "800-900:D", "120-124:A", "200-204:A", "260-300:A",
          "500-504:A", "600-640:A", "900-1000:B", "1000-1004:C", "1050-1150:A", "1100-1250:R", "1250-1500:L",
          "1300-1304:A", "1400-1440:A"]
SCENES = ["fighters", "title"]
KINDS = ["none", "dma", "prof", "both", "loops"]
INTRO = {
    "none": "It runs at {fps} game loops per second instead of 60 on an emulated NTSC console ({dropped} of {frames} "
            "frames dropped).",
    "dma": "Below is the report of a checker that ran it for {total} frames on an emulated NTSC console and logged "
           "every DMA transfer.",
    "prof": "Below is the report of a profiler that ran it for {total} frames on an emulated NTSC console and "
            "charged every 68000 cycle to a function.",
    "both": "Below is the report of two checkers that ran it for {total} frames on an emulated NTSC console: one "
            "logged every DMA transfer, the other charged every 68000 cycle to a function.",
    "loops": "Below is the report of two checkers that ran it for {total} frames on an emulated NTSC console: one "
             "logged every DMA transfer, the other charged every 68000 cycle to a function. The last part adds up "
             "the cost of each game loop.",
}
ROUND2 = """You changed the scene as asked. Here is your new main.c and how it ran. {intro} Fix the problems \
without changing what the scene shows: same objects, same sizes, same animations and behavior. Reply with the full \
main.c in one ```c code block.
{report}
main.c:
{code}"""
FILES = (("dma", "report-dma.txt"), ("prof", "report-prof.txt"), ("both", "report.txt"), ("loops", "report-loops.txt"))
FIX = """Here is a Mega Drive scene written with SGDK 2.11. It must run at a steady 60 frames per second on a real \
NTSC console. {intro} Fix the problems without changing what the scene shows: same objects, same sizes, same \
animations and behavior. Reply with the full main.c in one ```c code block.
{report}
main.c:
{code}"""
FIX_BUILD = """This main.c does not compile with SGDK 2.11. Compiler output:

{errors}

Fix it and reply with the full corrected main.c in one ```c code block.

{code}"""


def log(*a):
    with lock:
        print(time.strftime("%H:%M:%S"), *a, flush=True)


def ask(model, prompt, cwd):
    for attempt in range(4):
        t0 = time.time()
        p = subprocess.run(["claude", "-p", "--model", model, "--system-prompt", SYSTEM, "--tools", "",
                            "--strict-mcp-config", "--setting-sources", "", "--output-format", "json"],
                           input=prompt, capture_output=True, text=True, env=ENV, cwd=cwd, timeout=1200)
        try:
            d = json.loads(p.stdout)
            if model not in d.get("modelUsage", {}) or d.get("is_error"):
                raise ValueError(f"model {list(d.get('modelUsage', {}))} error {d.get('is_error')}")
            d["wall_s"] = round(time.time() - t0, 2)
            return d
        except (ValueError, json.JSONDecodeError) as e:
            log(f"  retry ({e}; exit {p.returncode})")
            time.sleep(20 * (attempt + 1))
    raise RuntimeError("claude -p failed 4 times")


def extract_c(text):
    blocks = re.findall(r"```(?:c|C|cpp)?\s*\n(.*?)```", text, re.S)
    return max(blocks, key=len) if blocks else None


def measure(code, tag, dest, model, cost):
    """Build (fixing compile errors with Claude, up to 3 rounds) and run through mdperf."""
    rounds = 0
    while True:
        ok, errs, d = build(code, f"{tag}-{rounds}")
        if ok or rounds == 3:
            break
        rounds += 1
        (dest / f"build-errors-{rounds}.txt").write_text(errs)
        r = ask(model, FIX_BUILD.format(errors=errs, code=code), dest)
        cost.append(r)
        new = extract_c(r["result"])
        if not new:
            break
        code = new
    info = dict(builds=ok, build_rounds=rounds)
    texts = {}
    if ok:
        s, texts = mdperf.analyze(d, tag, FRAMES, FROM, INPUTS, SHOT, dest)
        info.update(s)
    return code, info, texts


def baseline(scene):
    dest = OUT / f"{scene}-baseline"
    if (dest / "result.json").exists():
        r = json.loads((dest / "result.json").read_text())
        return r, {k: (dest / f).read_text() for k, f in FILES}
    dest.mkdir(parents=True, exist_ok=True)
    code = (HERE / "scenes" / f"{scene}.c").read_text()
    _, info, texts = measure(code, f"{scene}-baseline", dest, None, [])
    shutil.copy(HERE / "scenes" / f"{scene}.c", dest / "main.c")
    (dest / "result.json").write_text(json.dumps(info, indent=1))
    return info, texts


def run_one(job):
    scene, kind, rep, model = job
    rid = f"{scene}-{kind}-{rep}"
    dest = OUT / rid
    if (dest / "result.json").exists():
        return json.loads((dest / "result.json").read_text())
    dest.mkdir(parents=True, exist_ok=True)
    base, texts = baseline(scene)
    code = (HERE / "scenes" / f"{scene}.c").read_text()
    intro = INTRO[kind].format(fps=base["fps"], dropped=base["dropped"], frames=base["frames"], total=FRAMES)
    report = "" if kind == "none" else f"\nReport:\n{texts[kind].strip()}\n"
    prompt = FIX.format(intro=intro, report=report, code=code)
    (dest / "prompt.txt").write_text(prompt)
    cost = []
    t0 = time.time()
    r = ask(model, prompt, dest)
    cost.append(r)
    (dest / "reply.md").write_text(r["result"])
    new = extract_c(r["result"])
    res = dict(id=rid, scene=scene, kind=kind, rep=rep, model=model)
    if new:
        new, info, _ = measure(new, rid, dest, model, cost)
        (dest / "main.c").write_text(new)
        res.update(info)
    else:
        res["error"] = "no code block in the reply"
    res["cost_usd"] = round(sum(c.get("total_cost_usd", 0) for c in cost), 4)
    res["calls"] = len(cost)
    res["output_tokens"] = sum(c.get("usage", {}).get("output_tokens", 0) for c in cost)
    res["claude_s"] = round(sum(c["wall_s"] for c in cost), 1)
    res["wall_s"] = round(time.time() - t0, 1)
    res["claude_code_version"] = cost[0].get("version") if cost else None
    (dest / "result.json").write_text(json.dumps(res, indent=1))
    log(rid, {k: res.get(k) for k in ("builds", "build_rounds", "fps", "mean_load", "max_queued", "spill_frames",
                                      "cost_usd", "error")})
    return res


def run_round2(job):
    """Second round: the run's own fixed main.c, re-measured, sent back with the same kind of report."""
    scene, kind, rep, model = job
    src = OUT / f"{scene}-{kind}-{rep}"
    rid = f"{scene}-{kind}-{rep}-r2"
    dest = OUT / rid
    if (dest / "result.json").exists():
        return json.loads((dest / "result.json").read_text())
    first = json.loads((src / "result.json").read_text())
    if not first.get("builds") or first.get("fps") == 60.0:
        return dict(id=rid, skipped=True, cost_usd=0, calls=0, output_tokens=0, claude_s=0)
    dest.mkdir(parents=True, exist_ok=True)
    code = (src / "main.c").read_text()
    # the same build, measured again so every report kind (including loops) is available
    _, info, texts = measure(code, f"{rid}-in", dest, model, [])
    for f in dest.glob("report*.txt"):
        f.rename(dest / ("in-" + f.name))
    for f in ("frames.csv", "shot.png"):
        if (dest / f).exists():
            (dest / f).rename(dest / ("in-" + f))
    intro = INTRO[kind].format(fps=info["fps"], dropped=info["dropped"], frames=info["frames"], total=FRAMES)
    report = "" if kind == "none" else f"\nReport:\n{texts[kind].strip()}\n"
    prompt = ROUND2.format(intro=intro, report=report, code=code)
    (dest / "prompt.txt").write_text(prompt)
    cost = []
    t0 = time.time()
    r = ask(model, prompt, dest)
    cost.append(r)
    (dest / "reply.md").write_text(r["result"])
    new = extract_c(r["result"])
    res = dict(id=rid, scene=scene, kind=kind, rep=rep, round=2, model=model, input_fps=info["fps"])
    if new:
        new, info2, _ = measure(new, rid, dest, model, cost)
        (dest / "main.c").write_text(new)
        res.update(info2)
    else:
        res["error"] = "no code block in the reply"
    res["cost_usd"] = round(sum(c.get("total_cost_usd", 0) for c in cost), 4)
    res["calls"] = len(cost)
    res["output_tokens"] = sum(c.get("usage", {}).get("output_tokens", 0) for c in cost)
    res["claude_s"] = round(sum(c["wall_s"] for c in cost), 1)
    res["wall_s"] = round(time.time() - t0, 1)
    (dest / "result.json").write_text(json.dumps(res, indent=1))
    log(rid, {k: res.get(k) for k in ("input_fps", "builds", "build_rounds", "fps", "mean_load", "max_loop_cycles",
                                      "cost_usd", "error")})
    return res


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--model", default="claude-sonnet-5-5")
    ap.add_argument("--reps", type=int, default=3)
    ap.add_argument("--workers", type=int, default=3)
    ap.add_argument("--only", help="comma-separated run ids, e.g. fighters-prof-1")
    ap.add_argument("--scenes", default=",".join(SCENES))
    ap.add_argument("--kinds", default=",".join(KINDS))
    ap.add_argument("--round2", action="store_true", help="second round for the first-round runs below 60")
    a = ap.parse_args()
    OUT.mkdir(exist_ok=True)
    scenes, kinds = a.scenes.split(","), a.kinds.split(",")
    for s in scenes:
        log(s, "baseline", baseline(s)[0])
    jobs = [(s, k, r, a.model) for r in range(1, a.reps + 1) for s in scenes for k in kinds]
    if a.only:
        jobs = [j for j in jobs if f"{j[0]}-{j[1]}-{j[2]}" in a.only.split(",")]
    with ThreadPoolExecutor(a.workers) as ex:
        results = list(ex.map(run_round2 if a.round2 else run_one, jobs))
    log("total USD", round(sum(r.get("cost_usd", 0) for r in results), 2))


main()
