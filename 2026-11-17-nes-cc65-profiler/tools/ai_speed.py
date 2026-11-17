#!/usr/bin/env python3
"""Can Claude make a 30 fps NES scene run at 60 fps, with and without the profile? (MIT)

The four Claude-written scenes that miss every other frame (boss-plain v0,
boss-warned v0, formation-warned v0, rain-plain v1) go to `claude -p` (tools
off, no CLAUDE.md, no settings, the same system prompt as the 11-13 and 11-15
tests) in two conditions, twice each:
  plain    the code, nes.h, and "it runs at 30 fps, make it 60"
  profile  the same plus the nesprof report for that scene
  lines    the same plus the report and the 15 hottest C lines (tools/lines.py,
           runtime helper cycles counted on the line that called them)
  spikes   the same as lines plus the 5 slowest game loops with their 3
           hottest lines each (nesprof --spikes)
  loop-plain   as plain, then up to 3 more rounds in the same session: if the
               build still misses frames, Claude gets the count and the busy
               cycles per loop (average, max) of its own version
  loop-spikes  as spikes, then up to 3 more rounds with the full report
               (functions, lines, slowest loops) of its own version
The answer is built (compile errors sent back, up to 3 rounds), profiled with
the same pad script, and checked for the same picture: run without input next
to the original, the sequence of sprite tables must match (same_picture.py).
Results land in ai-speed/<scene>-<condition>-r<n>/.

usage: ai_speed.py [--model claude-sonnet-5-5] [--reps 2] [--workers 3] [--conditions plain,profile,lines]
       [--only <scene>-<condition>-r<n>,...]
The runs in the post: the default (16 runs), then reps 3-6 of rain-plain plain
and profile, then rain-plain lines r1-r6, then rain-plain spikes r1-r6, then
rain-plain loop-plain and loop-spikes r1-r6.
"""
import argparse
import json
import os
import re
import subprocess
import threading
import time
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

import profile_scenes
import same_picture

HERE = profile_scenes.HERE
OUT = HERE / "ai-speed"
ENV = dict(os.environ, CLAUDE_CODE_DISABLE_CLAUDE_MDS="1", CLAUDE_CODE_DISABLE_AUTO_MEMORY="1")
SYSTEM = "You write C code for the NES (Nintendo Entertainment System) using the cc65 compiler."
SCENES = {"boss-plain": "v0", "boss-warned": "v0", "formation-warned": "v0", "rain-plain": "v1"}
TASK = """Here is an NES scene written in C for cc65 (compiled with cl65 -t nes -Oirs). On the NES it runs at 30 \
frames per second instead of 60: its game loop takes longer than one frame, so it misses every other frame. Make it \
run at 60 fps without changing what the scene shows: same objects, same sizes, same positions and movement in every \
game loop. nes.h and nes.c are a fixed library and cannot change. Reply with the full main.c in one ```c code block.
{profile}
nes.h:
{nesh}

main.c:
{code}"""
PROFILE = """
Profiler report (nesprof, run in an emulator over 1,200 frames with a scripted pad; "self" is a function's own \
cycles, "incl" adds what it calls, names without a leading underscore are cc65 runtime helpers or the startup code, \
busy% is the share of the cycles not spent waiting in wait_frame; one NTSC frame is about 29,780 CPU cycles):

{report}
"""
LINES = """
Hottest C lines (cycles per frame; a line's cycles include the cc65 runtime helpers it called, such as pushax or \
shlax2):

{lines}
"""
SPIKES = """
The 5 slowest game loops (busy cycles from one wait_frame return to the next; a loop over about 29,000 busy cycles \
misses a frame), with the 3 hottest lines in each:

{spikes}
"""
FIX_BUILD = """This main.c does not compile with cc65 (cl65 -t nes -Oirs, with nes.h and nes.c from the library). \
Compiler output:

{errors}

Fix it and reply with the full corrected main.c in one ```c code block.

{code}"""
lock = threading.Lock()


def log(*a):
    with lock:
        print(time.strftime("%H:%M:%S"), *a, flush=True)


ROUND = """I built your main.c and ran it in the emulator with the same scripted pad. It still misses frames: \
{slow} of {loops} game loops took longer than one frame (busy cycles per loop: average {avg:,}, max {max:,}; one NTSC \
frame is about 29,780 CPU cycles). Make it run at 60 fps without changing what the scene shows. Reply with the full \
main.c in one ```c code block.
{report}"""


def ask(model, prompt, cwd, resume=None):
    for attempt in range(4):
        t0 = time.time()
        p = subprocess.run(["claude", "-p", "--model", model, "--system-prompt", SYSTEM, "--tools", "",
                            "--strict-mcp-config", "--setting-sources", "", "--output-format", "json"]
                           + (["--resume", resume] if resume else []),
                           input=prompt, capture_output=True, text=True, env=ENV, cwd=cwd, timeout=900)
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


def build(code, rom):
    rom.parent.mkdir(parents=True, exist_ok=True)
    src = rom.with_suffix(".c")
    src.write_text(code)
    p = subprocess.run([str(HERE / "nes/build_rom.sh"), str(rom), str(src)], capture_output=True, text=True,
                       timeout=300)
    out = p.stdout + p.stderr
    errs = [l for l in out.splitlines() if re.search(r"error|unresolved|undefined", l, re.I)]
    return p.returncode == 0, "\n".join(errs[:40]) if errs else out[-3000:]


def oam_run(rom, out):
    rel = lambda p: str(Path(p).relative_to(HERE))
    subprocess.run([str(HERE / "checker/nesprof.sh"), rel(rom), "700", "--oam", rel(out)], check=True,
                   capture_output=True, cwd=HERE)


def report_for(scene, stage):
    txt = (HERE / "scenes" / scene / f"{stage}-nesprof.txt").read_text()
    return re.sub(r", wall [^\n]*", "", txt).strip()


def run_one(job):
    scene, cond, rep, model = job
    dest = OUT / f"{scene}-{cond}-r{rep}"
    if (dest / "result.json").exists():
        return json.loads((dest / "result.json").read_text())
    dest.mkdir(parents=True, exist_ok=True)
    stage = SCENES[scene]
    code = (HERE / "scenes" / scene / f"{stage}.c").read_text()
    base = cond.replace("loop-", "")
    prof = PROFILE.format(report=report_for(scene, stage)) if base in ("profile", "lines", "spikes") else ""
    if base in ("lines", "spikes"):
        prof += LINES.format(lines=(HERE / "scenes" / scene / f"{stage}-lines.md").read_text().strip())
    if base == "spikes":
        prof += SPIKES.format(spikes=(HERE / "scenes" / scene / f"{stage}-spikes.md").read_text().strip())
    prompt = TASK.format(profile=prof, nesh=(HERE / "nes/nes.h").read_text(), code=code)
    (dest / "prompt.txt").write_text(prompt)
    t0 = time.time()
    cost = [ask(model, prompt, dest)]
    (dest / "answer.md").write_text(cost[0]["result"])
    new = extract_c(cost[0]["result"]) or ""
    scratch = HERE / ".scratch" / "ai-speed" / dest.name
    rounds = 0
    ok, errs = build(new, scratch / "fix.nes")
    while not ok and rounds < 3:
        rounds += 1
        (dest / f"build-errors-{rounds}.txt").write_text(errs)
        r = ask(model, FIX_BUILD.format(errors=errs, code=new), dest)
        cost.append(r)
        new = extract_c(r["result"]) or new
        ok, errs = build(new, scratch / "fix.nes")
    (dest / "fix.c").write_text(new)
    rounds_log = []
    while ok and cond.startswith("loop-") and len(rounds_log) < 3:
        a = profile_scenes.profile(scratch / "fix.nes", dest, "fix")
        rounds_log.append({k: a[k] for k in ("slow_loops", "loops", "busy_per_loop_avg", "busy_per_loop_max")})
        if a["slow_loops"] == 0:
            break
        rep_txt = ""
        if base == "spikes":
            rep_txt = PROFILE.format(report=re.sub(r", wall [^\n]*", "", (dest / "fix-nesprof.txt").read_text()).strip())
            rep_txt += LINES.format(lines=(dest / "fix-lines.md").read_text().strip())
            rep_txt += SPIKES.format(spikes=(dest / "fix-spikes.md").read_text().strip())
        msg = ROUND.format(slow=a["slow_loops"], loops=a["loops"], avg=a["busy_per_loop_avg"],
                           max=a["busy_per_loop_max"], report=rep_txt)
        (dest / f"round-{len(rounds_log)}.txt").write_text(msg)
        r = ask(model, msg, dest, resume=cost[-1]["session_id"])
        cost.append(r)
        (dest / f"answer-round-{len(rounds_log)}.md").write_text(r["result"])
        new = extract_c(r["result"]) or new
        ok, errs = build(new, scratch / "fix.nes")
        b = 0
        while not ok and b < 3:
            b += 1
            r = ask(model, FIX_BUILD.format(errors=errs, code=new), dest, resume=cost[-1]["session_id"])
            cost.append(r)
            new = extract_c(r["result"]) or new
            ok, errs = build(new, scratch / "fix.nes")
        (dest / "fix.c").write_text(new)
    before = json.loads((HERE / "results" / "scenes-summary.json").read_text())[f"{scene}/{stage}"]
    res = dict(scene=scene, condition=cond, rep=rep, model=model, from_stage=stage, build_rounds=rounds, builds=ok,
               before={k: before[k] for k in ("loops", "slow_loops", "busy_per_loop_avg", "runtime_share")},
               rounds=rounds_log)
    if ok:
        res["after"] = profile_scenes.profile(scratch / "fix.nes", dest, "fix")
        orig = HERE / ".scratch" / scene / f"{stage}.nes"
        oam_run(orig, scratch / "orig-oam.csv")
        oam_run(scratch / "fix.nes", scratch / "fix-oam.csv")
        res["picture"] = same_picture.compare(scratch / "orig-oam.csv", scratch / "fix-oam.csv")
    res["cost_usd"] = round(sum(c.get("total_cost_usd", 0) for c in cost), 4)
    res["calls"] = len(cost)
    res["output_tokens"] = sum(c.get("usage", {}).get("output_tokens", 0) for c in cost)
    res["model_wall_s"] = round(sum(c["wall_s"] for c in cost), 1)
    res["wall_s"] = round(time.time() - t0, 1)
    (dest / "result.json").write_text(json.dumps(res, indent=1))
    a = res.get("after", {})
    log(scene, cond, rep, "builds", ok, "slow", before["slow_loops"], "->", a.get("slow_loops"), "busy",
        before["busy_per_loop_avg"], "->", a.get("busy_per_loop_avg"), "same", res.get("picture", {}).get("same"),
        "USD", res["cost_usd"])
    return res


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--model", default="claude-sonnet-5-5")
    ap.add_argument("--reps", type=int, default=2)
    ap.add_argument("--workers", type=int, default=3)
    ap.add_argument("--only")
    ap.add_argument("--conditions", default="plain,profile")
    a = ap.parse_args()
    OUT.mkdir(exist_ok=True)
    jobs = [(s, c, r, a.model) for s in SCENES for c in a.conditions.split(",") for r in range(1, a.reps + 1)]
    if a.only:
        jobs = [j for j in jobs if f"{j[0]}-{j[1]}-r{j[2]}" in a.only.split(",")]
    with ThreadPoolExecutor(a.workers) as ex:
        results = list(ex.map(run_one, jobs))
    if not a.only:
        (OUT / "summary.json").write_text(json.dumps(results, indent=1))
    log("total USD", round(sum(r.get("cost_usd", 0) for r in results), 2))


if __name__ == "__main__":
    main()
