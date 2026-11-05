"""Do Claude's Mega Drive scenes fit the per-line sprite limits, and can it fix them from the report?

For each scene x prompt (plain, warned):
  1. ask `claude -p` (tools off, no CLAUDE.md, no settings) for a complete SGDK main.c
  2. build it; on errors send them back (up to 3 rounds)
  3. run it headless in mdsprites (scripted pad, limits off then on)
  4. if it lost sprite pixels, send the mdsprites report back once and check the new version
Everything lands in ai-test/<id>/. usage: ai_test.py [--model claude-sonnet-5-5] [--workers 3]
"""
import argparse
import csv
import json
import os
import re
import shutil
import subprocess
import sys
import threading
import time
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

HERE = Path(__file__).resolve().parent.parent
WORK = Path(os.environ.get("WORK", Path.home() / ".cache/mdsprites"))
SDK = Path(os.environ.get("SGDK_DIR", WORK / "sgdk-v2.11"))
IMG = os.environ.get("SGDK_IMAGE", "ghcr.io/stephane-d/sgdk:latest")
OUT = HERE / "ai-test"
ENV = dict(os.environ, WORK=str(WORK), CLAUDE_CODE_DISABLE_CLAUDE_MDS="1", CLAUDE_CODE_DISABLE_AUTO_MEMORY="1")
SYSTEM = "You write C code for the Sega Mega Drive / Genesis using SGDK."
UID = f"{os.getuid()}:{os.getgid()}"
lock = threading.Lock()
FRAMES, FROM = 1260, 60
INPUTS = ["70-74:S", "100-400:R", "400-700:L", "700-800:U", "800-900:D", "120-124:A", "200-204:A", "260-300:A",
          "500-504:A", "600-640:A", "900-1000:B", "1000-1004:C", "1050-1150:A", "1100-1250:R"]

SCENES = {
    "formation": "a shoot-em-up scene: three rows of 12 enemy ships, each 24x24 pixels, marching left and right "
                 "together as a formation; a player ship moves along the bottom with the d-pad.",
    "rings": "a bullet pattern: an enemy in the center of the screen fires an expanding ring of 32 bullets (8x8) "
             "every 30 frames, up to 64 bullets on screen; the player moves with the d-pad.",
    "convoy": "a road: a convoy of 14 trucks, each 32x16 pixels, all on the same row of the screen, drives left "
              "to right and wraps around when a truck leaves the screen; the player moves with the d-pad.",
    "boss": "a boss fight: a 96x64 pixel boss swaying left and right near the top, with 20 missiles (16x16) that "
            "orbit it in a circle; the player moves with the d-pad along the bottom.",
    "brawler": "a beat-em-up scene: 10 enemies, each 32x48 pixels, walk back and forth along the same ground line "
               "at different speeds; the player (also 32x48) walks with the d-pad.",
    "stars": "a parallax starfield of 64 stars (8x8) moving left at 3 different speeds, drawn as hardware sprites, "
             "with a player ship (32x24) that moves with the d-pad.",
    "rain": "rain: 70 raindrops (8x16) fall at different speeds and restart at a random position at the top; an "
            "umbrella (48x32) moves with the d-pad at the bottom.",
    "snake": "a long snake made of 30 segments (16x16) that follows the player's head around the screen with the "
             "d-pad, each segment 8 frames behind the previous one; it starts moving right along the middle row.",
}
PROMPT = """Write a complete SGDK 2.11 program in one file, main.c, for the Sega Mega Drive: {task}

Constraints: no resource files (no rescomp); define any tiles as const u32 arrays in the code and load them with \
VDP_loadTileData. 320x224 screen, controls on pad 1.{extra} Reply with the full main.c in one ```c code block."""
WARNED = " It must look right on a real console, with no sprites dropping out or flickering."
FIX_BUILD = """This main.c does not compile with SGDK 2.11. Compiler output:

{errors}

Fix it and reply with the full corrected main.c in one ```c code block.

{code}"""
FIX_REPORT = """Here is a Mega Drive scene written with SGDK 2.11, and the report of a checker that ran it and walked \
the sprite list for every line of every frame. Fix the sprite losses without changing what the scene shows: same \
objects, same sizes, same behavior. Reply with the full main.c in one ```c code block.

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
    args = [str(HERE / "tools/rom_check.sh"), f"ai/{d.name}", str(FRAMES), "--shot", f"600:ai/{d.name}/shot.ppm"]
    for i in INPUTS:
        args += ["--input", i]
    p = subprocess.run(args, capture_output=True, text=True, env=ENV, timeout=300)
    (dest / f"{stage}-report.txt").write_text(p.stdout + p.stderr)
    if p.returncode or not (d / "frames.csv").exists():
        return dict(error=(p.stdout + p.stderr)[-400:]), ""
    ex = subprocess.run([sys.executable, str(HERE / "tools/explain.py"), str(d / "frames.csv"), str(d / "events.csv"),
                         "--from", str(FROM)], capture_output=True, text=True).stdout
    (dest / f"{stage}-explain.txt").write_text(ex)
    shutil.copy(d / "frames.csv", dest / f"{stage}-frames.csv")
    subprocess.run([sys.executable, str(HERE / "tools/ppm2png.py"), str(d / "shot.ppm"), "--out", str(dest)])
    if (dest / "shot.png").exists():
        (dest / "shot.png").rename(dest / f"{stage}-shot.png")
    rows = [r for r in csv.DictReader(open(d / "frames.csv")) if int(r["frame"]) >= FROM]
    hit = [r for r in rows if int(r["sprites_lost"])]
    loads = sorted(float(r["load_pct"]) for r in rows)
    return dict(frames=len(rows), frames_lost=len(hit), pct_lost=round(100 * len(hit) / len(rows), 1),
                max_sprites_lost=max([int(r["sprites_lost"]) for r in rows] + [0]),
                real_lines=sum(int(r["real_lines"]) for r in rows), real_pixels=sum(int(r["real_pixels"]) for r in rows),
                real_only=sum(int(r["fn"]) for r in rows), predicted_only=sum(int(r["fp"]) for r in rows),
                max_chain=max(int(r["chain"]) for r in rows), max_count=max(int(r["max_count"]) for r in rows),
                max_dots=max(int(r["max_dots"]) for r in rows), flag_frames=sum(int(r["flag"]) for r in hit),
                off_frames=sum(1 for r in hit if int(r["off_slots_on_loss_lines"])),
                late_frames=sum(1 for r in rows if int(r["late_sat_writes"])), p95_load=loads[int(0.95 * (len(loads) - 1))],
                peak_load=loads[-1]), ex


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
    ex = ""
    if ok:
        res, ex = check(d, dest, stage)
        info.update(res)
    return code, info, ex


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
    code, v0, ex = evaluate(code, rid, dest, "v0", model, cost)
    res = dict(id=rid, scene=scene, variant=variant, model=model, v0=v0)
    log(rid, "v0", {k: v0.get(k) for k in ("builds", "build_rounds", "pct_lost", "max_sprites_lost", "max_chain", "error")})
    if v0.get("frames_lost"):
        r = ask(model, FIX_REPORT.format(report=ex, code=code), dest)
        cost.append(r)
        new = extract_c(r["result"])
        if new:
            _, v1, _ = evaluate(new, rid, dest, "v1", model, cost)
            res["v1"] = v1
            log(rid, "v1", {k: v1.get(k) for k in ("builds", "pct_lost", "max_sprites_lost", "max_chain", "error")})
    res["cost_usd"] = round(sum(c.get("total_cost_usd", 0) for c in cost), 4)
    res["input_tokens"] = sum(c.get("usage", {}).get("input_tokens", 0) for c in cost)
    res["output_tokens"] = sum(c.get("usage", {}).get("output_tokens", 0) for c in cost)
    res["wall_s"] = round(time.time() - t0, 1)
    (dest / "result.json").write_text(json.dumps(res, indent=1))
    return res


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--model", default="claude-sonnet-5-5")
    ap.add_argument("--workers", type=int, default=3)
    a = ap.parse_args()
    OUT.mkdir(exist_ok=True)
    jobs = [(s, v, a.model) for s in SCENES for v in ("plain", "warned")]
    with ThreadPoolExecutor(a.workers) as ex:
        results = list(ex.map(run_one, jobs))
    (OUT / "summary.json").write_text(json.dumps(results, indent=1))
    log("total USD", round(sum(r.get("cost_usd", 0) for r in results), 2))


main()
