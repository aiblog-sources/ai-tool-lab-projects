"""Do Claude's Mega Drive scenes that stream graphics fit the DMA budget, and can it fix them from the report?

For each scene x prompt (plain, warned):
  1. ask `claude -p` (tools off, no CLAUDE.md, no settings) for a complete SGDK main.c
  2. build it with SGDK 2.11; on errors send them back (up to 3 rounds)
  3. run it headless in mddma for FRAMES frames with a scripted pad (NTSC)
  4. if it went over budget (DMA during the display, bytes dropped or game frames lost), send the mddma
     report back once and check the new version
Everything lands in ai-test/<id>/. usage: ai_test.py [--model claude-sonnet-5-5] [--workers 3]
"""
import argparse
import csv
import json
import os
import re
import shutil
import statistics
import subprocess
import sys
import threading
import time
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

HERE = Path(__file__).resolve().parent.parent
WORK = Path(os.environ.get("WORK", Path.home() / ".cache/mddma"))
SDK = Path(os.environ.get("SGDK_DIR", WORK / "sgdk-v2.11"))
IMG = os.environ.get("SGDK_IMAGE", "ghcr.io/stephane-d/sgdk:latest")
OUT = HERE / "ai-test"
ENV = dict(os.environ, CLAUDE_CODE_DISABLE_CLAUDE_MDS="1", CLAUDE_CODE_DISABLE_AUTO_MEMORY="1")
SYSTEM = "You write C code for the Sega Mega Drive / Genesis using SGDK."
UID = f"{os.getuid()}:{os.getgid()}"
lock = threading.Lock()
FRAMES, FROM = 1500, 360
INPUTS = ["70-74:S", "100-400:R", "400-700:L", "700-800:U", "800-900:D", "120-124:A", "200-204:A", "260-300:A",
          "500-504:A", "600-640:A", "900-1000:B", "1000-1004:C", "1050-1150:A", "1100-1250:R", "1250-1500:L", "1300-1304:A", "1400-1440:A"]

SCENES = {
    "boss": "a boss fight: a 128x128 pixel boss with an 8-frame animation (it changes frame every 4 video frames) "
            "near the top of the screen, 16 bullets (8x8) falling from it, and a 32x32 player ship that moves with "
            "the d-pad along the bottom.",
    "fighters": "a fighting game scene: two fighters, each 96x112 pixels with a 6-frame idle animation and a "
                "6-frame punch animation (punch on button A for player 1); each fighter changes animation frame "
                "every 3 video frames; player 1 walks with the d-pad, player 2 stands still.",
    "title": "an animated title screen: a 256x160 pixel logo in the middle of a plane, made of 640 unique tiles, "
             "that cycles through 4 animation frames (a new frame every 8 video frames), with a 'PRESS START' "
             "text that blinks below it.",
    "brawler": "a beat-em-up scene: 5 characters, each 48x64 pixels with their own 8-frame walk cycle (each changes "
               "frame every 6 video frames, not in step with each other), walking back and forth; the player is one "
               "of them and moves with the d-pad.",
    "scroller": "a platformer background: a 2048x512 pixel level map on plane B that scrolls in both directions "
                "with the d-pad (up to 4 pixels per frame), plus a plane A cloud layer with per-line horizontal "
                "parallax (a different scroll speed for each band of 8 lines).",
}
PROMPT = """Write a complete SGDK 2.11 program in one file, main.c, for the Sega Mega Drive: {task}

Constraints: no resource files (no rescomp); make all graphics in the code (const u32 tile arrays, or tiles \
generated into RAM at startup) and draw them however you like. 320x224 screen, controls on pad 1.{extra} Reply \
with the full main.c in one ```c code block."""
WARNED = " It must run at a steady 60 frames per second on a real NTSC console, with no graphics glitches."
FIX_BUILD = """This main.c does not compile with SGDK 2.11. Compiler output:

{errors}

Fix it and reply with the full corrected main.c in one ```c code block.

{code}"""
FIX_REPORT = """Here is a Mega Drive scene written with SGDK 2.11, and the report of a checker that ran it for \
{frames} frames on an emulated NTSC console and logged every DMA transfer. Fix the problems the report shows \
without changing what the scene shows: same objects, same sizes, same animations and behavior. Reply with the full \
main.c in one ```c code block.

Checker report:
{report}

main.c:
{code}"""


def log(*a):
    with lock:
        print(time.strftime("%H:%M:%S"), *a, flush=True)


def ask(model, prompt, cwd):
    for attempt in range(4):
        t0 = time.time()
        p = subprocess.run(["claude", "-p", "--model", model, "--system-prompt", SYSTEM, "--tools", "",
                            "--strict-mcp-config", "--setting-sources", "", "--output-format", "json"],
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


def build(code, tag):
    d = WORK / "ai" / tag
    if d.exists():
        shutil.rmtree(d)
    (d / "src").mkdir(parents=True)
    (d / "src" / "main.c").write_text(code)
    mounts = []
    for x in ("inc", "lib", "res", "src"):
        mounts += ["-v", f"{SDK / x}:/sgdk/{x}:ro"]
    mounts += ["-v", f"{SDK / 'bin/rescomp.jar'}:/sgdk/bin/rescomp.jar:ro"]
    p = subprocess.run(["docker", "run", "--rm", "--network", "none", "--user", UID] + mounts + ["-v", f"{d}:/src", IMG],
                       capture_output=True, text=True, timeout=300)
    out = p.stdout + p.stderr
    ok = (d / "out/rom.bin").exists() and (d / "out/symbol.txt").exists()
    errs = [l for l in out.splitlines() if re.search(r"error|undefined reference", l, re.I)]
    return ok, "\n".join(errs[:40]) if errs else out[-3000:], d


def check(d, dest, stage):
    rel = d.relative_to(WORK)
    args = ["docker", "run", "--rm", "--network", "none", "--user", UID, "-v", f"{WORK}:/w", "-w", "/w", "gcc:14",
            "./mddma", "gpgx/genesis_plus_gx_libretro.so", f"{rel}/out/rom.bin", f"{rel}/out/symbol.txt", str(FRAMES),
            "--from", str(FROM), "--csv", f"{rel}/frames.csv", "--shot", f"600:{rel}/shot.ppm"]
    for i in INPUTS:
        args += ["--input", i]
    p = subprocess.run(args, capture_output=True, text=True, timeout=600)
    report = p.stdout + p.stderr
    (dest / f"{stage}-report.txt").write_text(report)
    if p.returncode or not (d / "frames.csv").exists():
        return dict(error=report[-400:]), report
    shutil.copy(d / "frames.csv", dest / f"{stage}-frames.csv")
    subprocess.run([sys.executable, str(HERE / "tools/ppm2png.py"), str(d / "shot.ppm"), "--out", str(dest)])
    if (dest / "shot.png").exists():
        (dest / "shot.png").rename(dest / f"{stage}-shot.png")
    return summarize(d / "frames.csv"), report


def summarize(path):
    rows = [r for r in csv.DictReader(open(path)) if int(r["frame"]) >= FROM]
    g = lambda k: [int(r[k]) for r in rows]
    fl = [r for r in rows if int(r["flushes"]) > 0]
    q = [int(r["queued"]) for r in fl]
    sent = sum(g("sent_vblank")) + sum(g("sent_active"))
    loads = sorted(float(r["load_pct"]) for r in rows)
    return dict(frames=len(rows), loops=len(fl), fps=round(60 * len(fl) / len(rows), 1), dropped=sum(g("dropped")),
                max_queued=max(q + [0]), median_queued=statistics.median(q) if q else 0,
                frames_over_6900=sum(1 for x in q if x > 6900), spill_frames=sum(1 for x in g("sent_active") if x),
                max_spill_bytes=max(g("sent_active") + [0]), last_spill_line=max(g("last_active_line") + [-1]),
                sat_late_frames=sum(1 for x in g("sat_line") if x >= 0),
                cram_late_frames=sum(1 for x in g("cram_line") if x >= 0),
                carry_frames=sum(g("carry")), short_bytes=max(0, sum(g("queued")) - sent),
                cpu_vdp_bytes_per_frame=round(statistics.mean(g("cpu_vdp_bytes"))),
                mean_load=round(statistics.mean(loads), 1), p95_load=loads[int(0.95 * (len(loads) - 1))])


def over_budget(v):
    return bool(v.get("spill_frames") or v.get("dropped") or v.get("short_bytes"))


def evaluate(code, tag, dest, stage, model, cost):
    rounds = 0
    while True:
        ok, errs, d = build(code, f"{tag}-{stage}-{rounds}")
        if ok or rounds == 3:
            break
        rounds += 1
        (dest / f"{stage}-build-errors-{rounds}.txt").write_text(errs)
        r = ask(model, FIX_BUILD.format(errors=errs, code=code), dest)
        cost.append(r)
        new = extract_c(r["result"])
        if not new:
            break
        code = new
    (dest / f"{stage}.c").write_text(code)
    info = dict(build_rounds=rounds, builds=ok)
    report = ""
    if ok:
        res, report = check(d, dest, stage)
        info.update(res)
    return code, info, report


def run_one(job):
    scene, variant, model = job
    rid = f"{scene}-{variant}"
    dest = OUT / rid
    if (dest / "result.json").exists():
        return json.loads((dest / "result.json").read_text())
    dest.mkdir(parents=True, exist_ok=True)
    cost = []
    t0 = time.time()
    extra = WARNED if variant == "warned" else ""
    r = ask(model, PROMPT.format(task=SCENES[scene], extra=extra), dest)
    cost.append(r)
    code = extract_c(r["result"]) or ""
    code, v0, report = evaluate(code, rid, dest, "v0", model, cost)
    res = dict(id=rid, scene=scene, variant=variant, model=model, v0=v0)
    keys = ("builds", "build_rounds", "fps", "max_queued", "spill_frames", "short_bytes", "sat_late_frames", "error")
    log(rid, "v0", {k: v0.get(k) for k in keys})
    if over_budget(v0):
        r = ask(model, FIX_REPORT.format(frames=FRAMES, report=report, code=code), dest)
        cost.append(r)
        new = extract_c(r["result"])
        if new:
            _, v1, _ = evaluate(new, rid, dest, "v1", model, cost)
            res["v1"] = v1
            log(rid, "v1", {k: v1.get(k) for k in keys})
    res["cost_usd"] = round(sum(c.get("total_cost_usd", 0) for c in cost), 4)
    res["calls"] = len(cost)
    res["input_tokens"] = sum(c.get("usage", {}).get("input_tokens", 0) for c in cost)
    res["output_tokens"] = sum(c.get("usage", {}).get("output_tokens", 0) for c in cost)
    res["wall_s"] = round(time.time() - t0, 1)
    (dest / "result.json").write_text(json.dumps(res, indent=1))
    return res


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--model", default="claude-sonnet-5-5")
    ap.add_argument("--workers", type=int, default=3)
    ap.add_argument("--only", help="comma-separated scene-variant ids")
    a = ap.parse_args()
    OUT.mkdir(exist_ok=True)
    jobs = [(s, v, a.model) for s in SCENES for v in ("plain", "warned")]
    if a.only:
        jobs = [j for j in jobs if f"{j[0]}-{j[1]}" in a.only.split(",")]
    with ThreadPoolExecutor(a.workers) as ex:
        results = list(ex.map(run_one, jobs))
    if not a.only:
        (OUT / "summary.json").write_text(json.dumps(results, indent=1))
    log("total USD", round(sum(r.get("cost_usd", 0) for r in results), 2))


main()
