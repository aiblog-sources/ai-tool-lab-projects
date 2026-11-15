"""Give Claude the nesflicker report and see whether the flicker gets better (MIT).

For each scene below, the latest version from 2026-11-13 (v1 if it had a fix
round, else v0) and its nesflicker report go to `claude -p` (tools off, no
CLAUDE.md, no settings, same system prompt as the 11-13 test), twice. The
answer is built (compile errors sent back, up to 3 rounds) and run through the
same check. Results land in ai-fix/<scene>-r<n>/.

usage: ai_fix.py [--model claude-sonnet-5-5] [--reps 2] [--workers 3]
"""
import argparse
import json
import os
import re
import subprocess
import sys
import threading
import time
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import claude_scenes  # noqa: E402

HERE = claude_scenes.HERE
SRC = HERE / "claude-scenes"
OUT = HERE / "ai-fix"
ENV = dict(os.environ, CLAUDE_CODE_DISABLE_CLAUDE_MDS="1", CLAUDE_CODE_DISABLE_AUTO_MEMORY="1")
SYSTEM = "You write C code for the NES (Nintendo Entertainment System) using the cc65 compiler."
# scenes whose shown share is below the best case, or where one sprite stays hidden 4+ frames
SCENES = {"brawler-plain": "v0", "brawler-warned": "v1", "convoy-plain": "v1", "formation-plain": "v1",
          "snake-warned": "v1"}
FIX = """Here is an NES scene written in C for cc65, and the report of a checker that ran it and followed every \
hardware sprite from frame to frame: how often each one was shown, how often it was hidden (dropped by the PPU's \
8-sprites-per-line limit, or left out of OAM by the program), and the longest stretch it stayed hidden. Reduce the \
flicker it reports without changing what the scene shows: same objects, same sizes, same behavior. Reply with the \
full main.c in one ```c code block.

Checker report:
{report}

main.c:
{code}"""
FIX_BUILD = """This main.c does not compile with cc65 (cl65 -t nes -Oirs, with nes.h and nes.c from the library). \
Compiler output:

{errors}

Fix it and reply with the full corrected main.c in one ```c code block.

{code}"""
lock = threading.Lock()


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


def compiles(code, tag):
    d = HERE / ".scratch" / f"build-{tag}"
    d.mkdir(parents=True, exist_ok=True)
    (d / "main.c").write_text(code)
    p = subprocess.run([str(HERE / "nes/build_rom.sh"), str(d / "rom.nes"), str(d / "main.c")],
                       capture_output=True, text=True, timeout=300)
    out = p.stdout + p.stderr
    errs = [l for l in out.splitlines() if re.search(r"error|unresolved|undefined", l, re.I)]
    return p.returncode == 0, "\n".join(errs[:40]) if errs else out[-3000:]


def run_one(job):
    scene, rep, model = job
    dest = OUT / f"{scene}-r{rep}"
    if (dest / "result.json").exists():
        return json.loads((dest / "result.json").read_text())
    dest.mkdir(parents=True, exist_ok=True)
    stage = SCENES[scene]
    code = (SRC / scene / f"{stage}.c").read_text()
    report = (SRC / scene / f"{stage}-flicker.txt").read_text()
    before = json.loads((SRC / scene / f"{stage}-flicker.json").read_text())["summary"]
    t0 = time.time()
    cost = [ask(model, FIX.format(report=report, code=code), dest)]
    new = extract_c(cost[0]["result"]) or ""
    (dest / "answer.md").write_text(cost[0]["result"])
    rounds = 0
    ok, errs = compiles(new, f"{scene}-r{rep}")
    while not ok and rounds < 3:
        rounds += 1
        (dest / f"build-errors-{rounds}.txt").write_text(errs)
        r = ask(model, FIX_BUILD.format(errors=errs, code=new), dest)
        cost.append(r)
        new = extract_c(r["result"]) or new
        ok, errs = compiles(new, f"{scene}-r{rep}")
    res = dict(scene=scene, rep=rep, model=model, from_stage=stage, before=before, build_rounds=rounds, builds=ok)
    if ok:
        src = dest / "fix-src.c"
        src.write_text(new)
        res["after"] = claude_scenes.run(src, dest, "fix")
        src.unlink()
    else:
        (dest / "fix.c").write_text(new)
    res["cost_usd"] = round(sum(c.get("total_cost_usd", 0) for c in cost), 4)
    res["calls"] = len(cost)
    res["output_tokens"] = sum(c.get("usage", {}).get("output_tokens", 0) for c in cost)
    res["wall_s"] = round(time.time() - t0, 1)
    (dest / "result.json").write_text(json.dumps(res, indent=1))
    a = res.get("after", {})
    log(scene, rep, "builds", ok, "shown", before["shown_pct"], "->", a.get("shown_pct"), "best", a.get("best_shown_pct"),
        "longest", before["longest_hidden"], "->", a.get("longest_hidden"), "USD", res["cost_usd"])
    return res


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--model", default="claude-sonnet-5-5")
    ap.add_argument("--reps", type=int, default=2)
    ap.add_argument("--workers", type=int, default=3)
    a = ap.parse_args()
    OUT.mkdir(exist_ok=True)
    jobs = [(s, r, a.model) for s in SCENES for r in range(1, a.reps + 1)]
    with ThreadPoolExecutor(a.workers) as ex:
        results = list(ex.map(run_one, jobs))
    (OUT / "summary.json").write_text(json.dumps(results, indent=1))
    log("total USD", round(sum(r.get("cost_usd", 0) for r in results), 2))


main()
