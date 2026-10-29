"""Does sgdklint catch the slow code Claude writes for the Mega Drive, and does fixing it help?

For each task x model x rep:
  1. ask `claude -p` (tools off, no CLAUDE.md, no settings) for a complete SGDK main.c
  2. build it with the SGDK Docker image; on errors, send the errors back (up to 3 rounds)
  3. lint it (lint/sgdklint.py) and profile it headless with mdprof (scripted pad, 1,260 frames)
  4. two fix rounds from the same v0 code: one with the lint output ("lint"), one with only
     "make it faster" ("plain"); each result is built, linted and profiled again
Everything lands in ai-test/<id>/. Needs Docker, the SGDK image and mdprof (see README).

usage: ai_test.py [--models claude-sonnet-5-5:2,claude-opus-5-5:1] [--tasks a,b] [--workers 3]
"""
import argparse
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
WORK = Path(os.environ.get("WORK", Path.home() / ".cache/mdprof"))
SGDK_IMAGE = os.environ.get("SGDK_IMAGE", "ghcr.io/stephane-d/sgdk:latest")
OUT = HERE / "ai-test"
ENV = dict(os.environ, CLAUDE_CODE_DISABLE_CLAUDE_MDS="1", CLAUDE_CODE_DISABLE_AUTO_MEMORY="1")
SYSTEM = "You write C code for the Sega Mega Drive / Genesis using SGDK."
UID = f"{os.getuid()}:{os.getgid()}"
lock = threading.Lock()

TASKS = {
    "starfield": "a parallax starfield of 64 stars moving left at 3 different speeds, drawn as hardware "
                 "sprites, with a player ship that moves with the d-pad.",
    "bullets": "a bullet pattern game: an enemy at the top fires bullets aimed at the player every 10 frames, "
               "up to 40 bullets on screen; the player dodges with the d-pad; show the number of hits.",
    "platformer": "a one-screen platformer: a player sprite with gravity, jump on A, and collision with a level "
                  "stored as a 40x28 array of solid/empty tiles that is drawn on plane A; show the score.",
    "snake": "snake on a 40x28 grid drawn with tiles on plane A; food appears at random places, the score is "
             "shown at the top, and the speed increases with the length.",
    "breakout": "breakout: a paddle on the d-pad, a ball, a 10x6 grid of bricks drawn with tiles, score and lives "
                "at the top.",
    "particles": "pressing A spawns an explosion of 48 particles (sprites) that fly out in all directions with "
                 "gravity and disappear after 1 second.",
    "homing": "a top-down arena: 8 enemies move toward the player at a constant speed (normalized direction), the "
              "player moves with the d-pad; show the score and a timer in seconds.",
    "scroller": "a side-scrolling background of procedurally generated hills on plane B that scrolls left forever, "
                "generating each new column of tiles as it scrolls in; a player sprite moves with the d-pad.",
    "hud": "a game HUD: a countdown timer in minutes:seconds, a score that increases by 10 every frame while A is "
           "held, lives, and a health bar drawn with tiles that shrinks while B is held.",
    "pong": "pong: the left paddle on the d-pad, the right paddle controlled by a simple AI, the ball speeds up 5% "
            "after each hit, scores at the top.",
}

PROMPT = """Write a complete SGDK 2.09 program in one file, main.c, for the Sega Mega Drive: {task}

Constraints: no resource files (no rescomp); define any tiles as const u32 arrays in the code and load them with \
VDP_loadTileData. 320x224 screen, controls on pad 1. Reply with the full main.c in one ```c code block."""

FIX_BUILD = """This main.c does not compile with SGDK 2.09. Compiler output:

{errors}

Fix it and reply with the full corrected main.c in one ```c code block.

{code}"""

FIX_LINT = """Here is a Mega Drive game written with SGDK 2.09, and the output of a performance lint for it. The \
cycle costs in the lint come from measurements on a 68000 (one NTSC frame is 128,005 cycles). Fix the findings \
without changing what the game does or how it looks. Reply with the full main.c in one ```c code block.

Lint output:
{lint}

main.c:
{code}"""

FIX_PLAIN = """Here is a Mega Drive game written with SGDK 2.09. Make it run faster on the real hardware (68000 \
at 7.67 MHz) without changing what the game does or how it looks. Reply with the full main.c in one ```c code \
block.

main.c:
{code}"""

# scripted pad: start taps, d-pad sweeps, A/B/C taps and holds (frames)
INPUTS = ["70-74:S", "100-400:R", "400-700:L", "700-800:U", "800-900:D", "120-124:A", "200-204:A",
          "260-300:A", "500-504:A", "600-640:A", "900-1000:B", "1000-1004:C", "1050-1150:A", "1100-1250:R",
          "300-304:S"]
FRAMES, FROM = 1260, 60


def log(*a):
    with lock:
        print(time.strftime("%H:%M:%S"), *a, flush=True)


def ask(model, prompt, cwd):
    for attempt in range(4):
        t0 = time.time()
        p = subprocess.run(
            ["claude", "-p", "--model", model, "--system-prompt", SYSTEM, "--tools", "", "--strict-mcp-config",
             "--setting-sources", "", "--output-format", "json"],
            input=prompt, capture_output=True, text=True, env=ENV, cwd=cwd, timeout=900)
        try:
            d = json.loads(p.stdout)
            if model not in d.get("modelUsage", {}) or d.get("is_error"):
                raise ValueError(f"model {list(d.get('modelUsage', {}))} error {d.get('is_error')}")
            d["wall_s"] = round(time.time() - t0, 2)
            return d
        except (ValueError, json.JSONDecodeError) as e:
            log(f"  retry ({e}; exit {p.returncode}) {p.stdout[:160]!r}")
            time.sleep(20 * (attempt + 1))
    raise RuntimeError("claude -p failed 4 times")


def extract_c(text):
    blocks = re.findall(r"```(?:c|C|cpp)?\s*\n(.*?)```", text, re.S)
    return max(blocks, key=len) if blocks else None


def build(code, tag):
    """returns (ok, errors, build dir)"""
    d = WORK / "ai" / tag
    if d.exists():
        shutil.rmtree(d)
    (d / "src").mkdir(parents=True)
    (d / "src" / "main.c").write_text(code)
    p = subprocess.run(["docker", "run", "--rm", "--network", "none", "--user", UID, "-v", f"{d}:/src", SGDK_IMAGE],
                       capture_output=True, text=True, timeout=300)
    out = p.stdout + p.stderr
    ok = (d / "out" / "rom.bin").exists() and (d / "out" / "symbol.txt").exists()
    errs = [l for l in out.splitlines() if re.search(r"error|undefined reference", l, re.I)]
    return ok, "\n".join(errs[:40]) if errs else out[-3000:], d


def profile(d):
    args = ["docker", "run", "--rm", "--network", "none", "--user", UID, "-v", f"{WORK}:/w", "-w", "/w", "gcc:14",
            "./mdprof", "gpgx/genesis_plus_gx_libretro.so", f"ai/{d.name}/out/rom.bin", f"ai/{d.name}/out/symbol.txt",
            str(FRAMES), "--from", str(FROM), "--csv", f"ai/{d.name}/frames.csv", "--json", f"ai/{d.name}/prof.json",
            "--shot", f"600:ai/{d.name}/shot.ppm"]
    for i in INPUTS:
        args += ["--input", i]
    p = subprocess.run(args, capture_output=True, text=True, timeout=300)
    if p.returncode != 0 or not (d / "prof.json").exists():
        return dict(error=(p.stdout + p.stderr)[-500:])
    j = json.loads((d / "prof.json").read_text())
    loads = []
    for line in (d / "frames.csv").read_text().splitlines()[1:]:
        c = line.split(",")
        if int(c[0]) >= FROM:
            loads.append(float(c[3]))
    loads.sort()
    return dict(mean_load=j["mean_load_pct"], dropped=j["dropped_frames"], peak_load=loads[-1],
                p95_load=loads[int(0.95 * (len(loads) - 1))], dma_active_frames=j["dma_active_frames"],
                top=[(x["fn"], x["pct"]) for x in j["profile"][:8]], report=p.stdout)


def lint(d):
    p = subprocess.run([sys.executable, str(HERE / "lint" / "sgdklint.py"), "--json", "lint.json", "src/main.c"],
                       capture_output=True, text=True, cwd=d)
    rep = json.loads((d / "lint.json").read_text())
    return p.stdout, rep


def save(dest, name, text):
    (dest / name).write_text(text)


def evaluate(code, tag, dest, stage, model, cost):
    """build (with up to 3 repair rounds), lint, profile; files go to dest with a stage prefix"""
    rounds, info = 0, {}
    while True:
        ok, errs, d = build(code, f"{tag}-{stage}-{rounds}")
        if ok or rounds == 3:
            break
        rounds += 1
        save(dest, f"{stage}-build-errors-{rounds}.txt", errs)
        r = ask(model, FIX_BUILD.format(errors=errs, code=code), dest)
        cost.append(r)
        new = extract_c(r["result"])
        if not new:
            break
        code = new
    save(dest, f"{stage}.c", code)
    info["build_rounds"] = rounds
    info["builds"] = ok
    if not ok:
        save(dest, f"{stage}-build-errors-final.txt", errs)
        return code, info
    text, rep = lint(d)
    save(dest, f"{stage}-lint.txt", text)
    info["lint"] = {lvl: sum(1 for x in rep["findings"] if x["level"] == lvl) for lvl in ("hot", "frame", "guarded", "init")}
    info["lint_rules"] = sorted({x["rule"] for x in rep["findings"]})
    info["findings"] = [dict(line=x["line"], rule=x["rule"], level=x["level"], function=x["function"])
                        for x in rep["findings"]]
    prof = profile(d)
    if "report" in prof:
        save(dest, f"{stage}-profile.txt", prof.pop("report"))
        if (d / "shot.ppm").exists():
            shutil.copy(d / "shot.ppm", dest / f"{stage}-shot.ppm")
    info["profile"] = prof
    info["lint_text"] = text
    return code, info


def run_one(task, model, rep):
    tag = f"{task}-{model.replace('claude-', '')}-r{rep}"
    dest = OUT / tag
    if (dest / "result.json").exists():
        log("skip", tag)
        return json.loads((dest / "result.json").read_text())
    dest.mkdir(parents=True, exist_ok=True)
    cost = []
    log("start", tag)
    prompt = PROMPT.format(task=TASKS[task])
    r = ask(model, prompt, dest)
    cost.append(r)
    save(dest, "v0-reply.md", r["result"])
    code = extract_c(r["result"])
    res = dict(task=task, model=model, rep=rep)
    if not code:
        res["error"] = "no code block"
    else:
        code, v0 = evaluate(code, tag, dest, "v0", model, cost)
        res["v0"] = v0
        if v0.get("builds"):
            lint_text = v0.pop("lint_text")
            for kind in ("lint", "plain"):
                if kind == "lint" and not v0["findings"]:
                    continue
                prompt = (FIX_LINT.format(lint=lint_text.strip(), code=code) if kind == "lint"
                          else FIX_PLAIN.format(code=code))
                r = ask(model, prompt, dest)
                cost.append(r)
                new = extract_c(r["result"])
                if not new:
                    res[kind] = dict(error="no code block")
                    continue
                _, info = evaluate(new, tag, dest, kind, model, cost)
                info.pop("lint_text", None)
                res[kind] = info
    res["calls"] = len(cost)
    res["cost_usd"] = round(sum(c.get("total_cost_usd", 0) for c in cost), 4)
    res["claude_wall_s"] = round(sum(c["wall_s"] for c in cost), 1)
    res["output_tokens"] = sum(c.get("usage", {}).get("output_tokens", 0) for c in cost)
    (dest / "result.json").write_text(json.dumps(res, indent=1))
    p = res.get("v0", {}).get("profile", {})
    log("done", tag, f"builds={res.get('v0', {}).get('builds')} rounds={res.get('v0', {}).get('build_rounds')}",
        f"lint={res.get('v0', {}).get('lint')} load={p.get('mean_load')} drop={p.get('dropped')} ${res['cost_usd']}")
    return res


def run_lint2(tag):
    """second lint version: lint the stored v0.c again and run one more fix round from its output"""
    dest = OUT / tag
    res = json.loads((dest / "result.json").read_text())
    if "lint2" in res or not res.get("v0", {}).get("builds"):
        return res
    tmp = WORK / "ai" / f"{tag}-v0lint2"
    if tmp.exists():
        shutil.rmtree(tmp)
    (tmp / "src").mkdir(parents=True)
    code = (dest / "v0.c").read_text()
    (tmp / "src" / "main.c").write_text(code)
    text, rep = lint(tmp)
    save(dest, "v0-lint2.txt", text)
    res["v0"]["lint2"] = {lvl: sum(1 for x in rep["findings"] if x["level"] == lvl)
                          for lvl in ("heavy", "hot", "frame", "guarded", "init")}
    res["v0"]["findings2"] = [dict(line=x["line"], rule=x["rule"], level=x["level"], function=x["function"],
                                   est_cycles=x["est_cycles"]) for x in rep["findings"]]
    cost = []
    if rep["findings"]:
        r = ask(res["model"], FIX_LINT.format(lint=text.strip(), code=code), dest)
        cost.append(r)
        new = extract_c(r["result"])
        if new:
            _, info = evaluate(new, tag, dest, "lint2", res["model"], cost)
            info.pop("lint_text", None)
            res["lint2"] = info
        else:
            res["lint2"] = dict(error="no code block")
    else:
        res["lint2"] = dict(skipped="no findings")
    res["calls"] += len(cost)
    res["cost_usd"] = round(res["cost_usd"] + sum(c.get("total_cost_usd", 0) for c in cost), 4)
    res["claude_wall_s"] = round(res["claude_wall_s"] + sum(c["wall_s"] for c in cost), 1)
    res["output_tokens"] += sum(c.get("usage", {}).get("output_tokens", 0) for c in cost)
    (dest / "result.json").write_text(json.dumps(res, indent=1))
    p = res["lint2"].get("profile", {})
    log("lint2", tag, res["v0"]["lint2"], f"load {res['v0']['profile']['mean_load']} -> {p.get('mean_load')}",
        f"drop {res['v0']['profile']['dropped']} -> {p.get('dropped')}")
    return res


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--models", default="claude-sonnet-5-5:2,claude-opus-5-5:1")
    ap.add_argument("--tasks", default=",".join(TASKS))
    ap.add_argument("--workers", type=int, default=3)
    ap.add_argument("--lint2", action="store_true", help="run the lint2 fix round on finished programs")
    a = ap.parse_args()
    if a.lint2:
        tags = sorted(p.parent.name for p in OUT.glob("*/result.json"))
        with ThreadPoolExecutor(a.workers) as ex:
            list(ex.map(run_lint2, tags))
        log("ALL DONE lint2", len(tags))
        return
    jobs = []
    for spec in a.models.split(","):
        m, n = spec.split(":")
        for rep in range(1, int(n) + 1):
            for t in a.tasks.split(","):
                jobs.append((t, m, rep))
    OUT.mkdir(exist_ok=True)
    with ThreadPoolExecutor(a.workers) as ex:
        results = list(ex.map(lambda j: run_one(*j), jobs))
    (HERE / "results" / "ai-test.json").write_text(json.dumps(results, indent=1))
    log("ALL DONE", len(results))


if __name__ == "__main__":
    main()
