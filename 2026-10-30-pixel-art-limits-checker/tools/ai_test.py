#!/usr/bin/env python3
"""Ask Claude models for pixel art as code for each console, check it with pixcheck, then try fix rounds.

For every (task, console, model):
  v0      `claude -p` (no tools, no CLAUDE.md, no settings, no MCP) writes a Pillow script; it runs in a Docker
          container with no network; pixcheck checks the PNG.
  If v0 fails, two separate fix arms start from v0:
  plain1  "this breaks the console's hardware limits, fix it" (no details)
  check1  the pixcheck report, then check2 with the new report if check1 still fails.

usage: ai_test.py [--variant limits|naive] [--models claude-sonnet-5-5,claude-opus-5-5,claude-haiku-5-5] [--consoles nes,sms,snes,md]
                  [--tasks landscape,dungeon,knight] [--workers 3]
Writes ai-test/<task>-<console>-<model>/ (scripts, PNGs, reports) and results/ai-test-<variant>.json. MIT license.
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

HERE = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(HERE / "pixcheck"))
import pixcheck  # noqa: E402

WORK = Path(os.environ.get("WORK", Path.home() / ".cache/pixcheck")) / "ai"
RUNNER = os.environ.get("RUNNER_IMAGE", "pixcheck-runner")   # python:3.12-slim + pillow 12.0.0 + numpy 2.3.4
ENV = dict(os.environ, CLAUDE_CODE_DISABLE_CLAUDE_MDS="1", CLAUDE_CODE_DISABLE_AUTO_MEMORY="1")
lock = threading.Lock()

SYSTEM = """You write Python 3 scripts that draw pixel art for retro console homebrew with Pillow (numpy is also \
available). Reply with one complete script in a single ```python code block. The script must save its output to \
the file name given in the request, in the current directory, and must not read files, use the network or ask for \
input."""

NAMES = {"nes": "NES", "sms": "Sega Master System", "snes": "SNES", "md": "Sega Mega Drive (Genesis)"}
SPRITE = {"nes": 16, "sms": 16, "snes": 32, "md": 32}
TASKS = {
    "landscape": "a background of a sunset over mountains and a lake",
    "dungeon": "a top-down dungeon room background: stone floor, stone walls, a wooden door and two torches",
    "knight": "a sprite sheet with a 4-frame walk cycle of a small knight facing right",
}


VARIANT = "limits"   # "limits": the prompt asks for the hardware limits; "naive": it only names the console


def prompt_for(task, console):
    name = NAMES[console]
    w, h = pixcheck.CONSOLES[console]["screen"]
    if task == "knight":
        f = SPRITE[console]
        limits = (f" It must work as-is on real {name} hardware as sprites: respect the console's color, palette "
                  "and tile limits.") if VARIANT == "limits" else ""
        return (f"Draw {TASKS[task]} for a {name} game. Each frame is {f}x{f} pixels; put the 4 frames side by side "
                f"in one {4 * f}x{f} PNG with a transparent background.{limits} Save it as art.png.")
    limits = (f" It must work as-is on real {name} hardware as one background layer: respect the console's color, "
              "palette and tile limits.") if VARIANT == "limits" else ""
    return f"Draw {TASKS[task]} for a {name} game, {w}x{h} pixels (the full screen).{limits} Save it as art.png."


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


def extract(text):
    blocks = re.findall(r"```(?:python|py)?\s*\n(.*?)```", text, re.S)
    return max(blocks, key=len) if blocks else None


def render(code, dest, stage):
    """Run the script in the runner container (no network, 2 minutes). Returns the PNG path or an error string."""
    d = dest / stage
    d.mkdir(parents=True, exist_ok=True)
    (d / "script.py").write_text(code or "")
    if not code:
        return None, "no python block in the reply"
    try:
        p = subprocess.run(["docker", "run", "--rm", "--network", "none", "--memory", "1g", "--cpus", "1",
                            "--user", f"{os.getuid()}:{os.getgid()}", "-v", f"{d}:/w", "-w", "/w", RUNNER,
                            "timeout", "120", "python", "-I", "script.py"], capture_output=True, text=True,
                           timeout=200)
    except subprocess.TimeoutExpired:
        return None, "timeout"
    (d / "run.log").write_text(p.stdout[-4000:] + p.stderr[-4000:])
    if p.returncode or not (d / "art.png").exists():
        return None, f"script failed (exit {p.returncode}): {p.stderr.strip().splitlines()[-1:] or ''}"
    return d / "art.png", None


def evaluate(code, task, console, dest, stage):
    png, err = render(code, dest, stage)
    if err:
        return {"stage": stage, "ran": False, "error": err}
    want = pixcheck.CONSOLES[console]["screen"] if task != "knight" else (4 * SPRITE[console], SPRITE[console])
    sprite = (SPRITE[console],) * 2 if task == "knight" else None
    rep, _, _ = pixcheck.check(console, str(png), sprite)
    text = pixcheck.text_report(rep).replace(str(png), "art.png")
    (dest / stage / "pixcheck.txt").write_text(text + "\n")
    rep["file"] = "art.png"
    (dest / stage / "pixcheck.json").write_text(json.dumps(rep, indent=1))
    pixcheck.overlay(str(png), rep, str(dest / stage / "overlay.png"))
    return {"stage": stage, "ran": True, "ok": rep["ok"], "size_ok": list(rep["size"]) == list(want),
            "size": rep["size"], "errors": rep["errors"], "warnings": rep["warnings"], "info": rep["info"],
            "bad_areas": len(rep["bad_areas"]), "unplaced": len(rep["unplaced"]), "report": text}


def fix_prompt(task, console, code, kind, report):
    name = NAMES[console]
    if kind == "plain":
        why = f"The image this script draws breaks {name} hardware limits, so it does not work as-is on the console."
    else:
        why = (f"A checker for {name} limits reports this about the image the script draws:\n\n{report}\n\n"
               "Coordinates are in pixels from the top-left corner.")
    return (f"Earlier request: {prompt_for(task, console)}\n\nYour script:\n```python\n{code}\n```\n\n{why}\n"
            "Fix the script so the image respects the limits while keeping the picture as close as possible. "
            "Reply with the complete corrected script.")


def one(task, console, model, out):
    tag = f"{VARIANT}-{task}-{console}-{model.replace('claude-', '')}"
    dest = WORK / tag
    dest.mkdir(parents=True, exist_ok=True)
    calls = []
    r = ask(model, prompt_for(task, console), dest)
    calls.append(r)
    code = extract(r["result"])
    res = {"tag": tag, "variant": VARIANT, "task": task, "console": console, "model": model}
    res["v0"] = evaluate(code, task, console, dest, "v0")
    log(tag, "v0", res["v0"].get("ok"), res["v0"].get("error") or res["v0"].get("errors"))
    if not res["v0"].get("ok") and code:
        report = res["v0"].get("report") or res["v0"].get("error")
        for kind in ("plain", "check"):
            prev_code, prev_report = code, report
            rounds = 1 if kind == "plain" else 2
            for n in range(1, rounds + 1):
                r = ask(model, fix_prompt(task, console, prev_code, kind, prev_report), dest)
                calls.append(r)
                new = extract(r["result"])
                stage = f"{kind}{n}"
                res[stage] = evaluate(new, task, console, dest, stage)
                log(tag, stage, res[stage].get("ok"), res[stage].get("error") or res[stage].get("errors"))
                if res[stage].get("ok") or not new:
                    break
                prev_code, prev_report = new, res[stage].get("report") or res[stage].get("error")
    res["calls"] = len(calls)
    res["cost_usd"] = round(sum(c.get("total_cost_usd", 0) for c in calls), 4)
    res["claude_wall_s"] = round(sum(c["wall_s"] for c in calls), 1)
    res["output_tokens"] = sum(c.get("usage", {}).get("output_tokens", 0) for c in calls)
    (dest / "result.json").write_text(json.dumps(res, indent=1))
    with lock:
        out.append(res)
    return res


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--models", default="claude-sonnet-5-5,claude-opus-5-5,claude-haiku-5-5")
    ap.add_argument("--consoles", default="nes,sms,snes,md")
    ap.add_argument("--tasks", default="landscape,dungeon,knight")
    ap.add_argument("--workers", type=int, default=3)
    ap.add_argument("--variant", choices=["limits", "naive"], default="limits")
    ap.add_argument("--out")
    a = ap.parse_args()
    global VARIANT
    VARIANT = a.variant
    jobs = [(t, c, m) for m in a.models.split(",") for t in a.tasks.split(",") for c in a.consoles.split(",")]
    out = []
    with ThreadPoolExecutor(a.workers) as ex:
        futs = [ex.submit(one, t, c, m, out) for t, c, m in jobs]
        for f in futs:
            try:
                f.result()
            except Exception as e:  # keep the other jobs going, record the failure
                log("job failed:", e)
    out.sort(key=lambda r: r["tag"])
    Path(a.out or HERE / "results" / f"ai-test-{a.variant}.json").write_text(json.dumps(out, indent=1))
    log(f"done: {len(out)} jobs, USD {sum(r['cost_usd'] for r in out):.2f}")


if __name__ == "__main__":
    main()
