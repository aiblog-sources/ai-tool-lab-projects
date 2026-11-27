#!/usr/bin/env python3
"""Ask Claude models to write the color conversion, then test what they wrote on every input.

Two tasks, each asked 3 times per model with `claude -p` (no tools, no CLAUDE.md, no settings, no MCP):
  snes  snes_to_md(color) -> int   SNES BGR555 word to Mega Drive CRAM word
  rgb   rgb_to_md(r, g, b) -> int  24-bit RGB to Mega Drive CRAM word
The code runs in the pixcheck-runner container with no network (tools/ai_eval.py). Writes ai-test/<tag>/ and
results/ai-test.json. Usage: ai_test.py [--models ...] [--reps 3]. MIT license."""
import argparse, json, os, re, subprocess, sys, threading, time
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

HERE = Path(__file__).resolve().parent.parent
ENV = dict(os.environ, CLAUDE_CODE_DISABLE_CLAUDE_MDS="1", CLAUDE_CODE_DISABLE_AUTO_MEMORY="1")
RUNNER = os.environ.get("RUNNER_IMAGE", "pixcheck-runner")
lock = threading.Lock()

SYSTEM = ("You write Python 3 code for retro console homebrew tools. Reply with one complete module in a single "
          "```python code block, standard library only, no I/O at import time.")
TASKS = {
    "snes": ("Write a Python function `snes_to_md(color: int) -> int` that converts a SNES color (the 15-bit BGR555 "
             "word) to the closest Sega Mega Drive (Genesis) CRAM color word. It is for a tool that ports SNES "
             "palettes to the Mega Drive."),
    "rgb": ("Write a Python function `rgb_to_md(r: int, g: int, b: int) -> int` that converts a 24-bit RGB color "
            "(0-255 per channel) to the closest Sega Mega Drive (Genesis) CRAM color word. It is for a tool that "
            "converts PNG art for a Mega Drive game."),
}


def log(*a):
    with lock:
        print(time.strftime("%H:%M:%S"), *a, flush=True)


def ask(model, prompt):
    for attempt in range(4):
        t0 = time.time()
        p = subprocess.run(
            ["claude", "-p", "--model", model, "--system-prompt", SYSTEM, "--tools", "", "--strict-mcp-config",
             "--setting-sources", "", "--output-format", "json"],
            input=prompt, capture_output=True, text=True, env=ENV, cwd="/tmp", timeout=900)
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


def run_one(model, task, rep):
    tag = f"{task}-{model.replace('claude-', '')}-r{rep}"
    d = HERE / "ai-test" / tag
    d.mkdir(parents=True, exist_ok=True)
    res = ask(model, TASKS[task])
    (d / "reply.md").write_text(res.get("result", ""))
    code = extract(res.get("result", "")) or ""
    (d / "solution.py").write_text(code)
    shutil_copy = (HERE / "tools/ai_eval.py").read_text()
    (d / "ai_eval.py").write_text(shutil_copy)
    p = subprocess.run(["docker", "run", "--rm", "--network", "none", "--user", f"{os.getuid()}:{os.getgid()}",
                        "-v", f"{d}:/w", "-w", "/w", RUNNER, "timeout", "120", "python3", "ai_eval.py", task],
                       capture_output=True, text=True)
    try:
        ev = json.loads(p.stdout)
    except json.JSONDecodeError:
        ev = {"error": (p.stdout + p.stderr)[-400:]}
    (d / "ai_eval.py").unlink()
    out = dict(tag=tag, model=model, task=task, rep=rep, cost_usd=round(res.get("total_cost_usd", 0), 4),
               wall_s=res["wall_s"], output_tokens=res.get("usage", {}).get("output_tokens"), eval=ev)
    (d / "eval.json").write_text(json.dumps(out, indent=1))
    log(tag, json.dumps(ev)[:200])
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--models", default="claude-sonnet-5-5,claude-opus-5-5,claude-haiku-5-5")
    ap.add_argument("--reps", type=int, default=3)
    a = ap.parse_args()
    jobs = [(m, t, r) for m in a.models.split(",") for t in TASKS for r in range(1, a.reps + 1)]
    with ThreadPoolExecutor(6) as ex:
        results = list(ex.map(lambda j: run_one(*j), jobs))
    json.dump(results, open(HERE / "results/ai-test.json", "w"), indent=1)
    print("total cost USD", round(sum(r["cost_usd"] for r in results), 3))


if __name__ == "__main__":
    main()
