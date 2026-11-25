#!/usr/bin/env python3
"""One more fix round on the 40 games of the 2026-10-29 test: the same "make it run faster" prompt as that test's
`plain` round, plus the sgdkhot report of the v0 code (measured cost per line, with the lint findings on it).
Same model as the one that wrote each game, `claude -p` with tools off, no CLAUDE.md, no settings; up to 3 build
repair rounds like the original test. The result is profiled by corpus.py as stage `hot`.

usage: fix_hot.py [--workers 3] [--only tag,tag]
"""
import argparse
import json
import os
import re
import shutil
import subprocess
import sys
import time
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import corpus  # noqa: E402
import sgdkhot  # noqa: E402

ENV = dict(os.environ, CLAUDE_CODE_DISABLE_CLAUDE_MDS="1", CLAUDE_CODE_DISABLE_AUTO_MEMORY="1")
SYSTEM = "You write C code for the Sega Mega Drive / Genesis using SGDK."  # as in 2026-10-29

FIX_HOT = """Here is a Mega Drive game written with SGDK 2.09. Make it run faster on the real hardware (68000 \
at 7.67 MHz) without changing what the game does or how it looks. Reply with the full main.c in one ```c code \
block.

Below is a line profile of this code, measured in an emulator with a scripted pad: cycles per video frame by \
source line (one NTSC frame is 128,005 cycles), what each line called, and the findings of a performance lint on \
the same lines.

{report}

main.c:
{code}"""

FIX_BUILD = """This main.c does not compile with SGDK 2.09. Compiler output:

{errors}

Fix it and reply with the full corrected main.c in one ```c code block.

{code}"""  # as in 2026-10-29


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
            print(f"  retry ({e}; exit {p.returncode}) {p.stdout[:160]!r}", flush=True)
            time.sleep(20 * (attempt + 1))
    raise RuntimeError("claude -p failed 4 times")


def extract_c(text):
    blocks = re.findall(r"```(?:c|C|cpp)?\s*\n(.*?)```", text, re.S)
    return max(blocks, key=len) if blocks else None


def try_build(code, tag):
    d = sgdkhot.WORK / "hot" / f"{tag}-hotbuild"
    if d.exists():
        shutil.rmtree(d)
    (d / "src").mkdir(parents=True)
    (d / "src" / "main.c").write_text(code)
    p = subprocess.run(["docker", "run", "--rm", "--network", "none", "--user", sgdkhot.UID, "-v", f"{d}:/src",
                        sgdkhot.IMG], capture_output=True, text=True, timeout=600)
    out = p.stdout + p.stderr
    ok = (d / "out" / "rom.bin").exists()
    errs = [l for l in out.splitlines() if re.search(r"error|undefined reference", l, re.I)]
    return ok, "\n".join(errs[:40]) if errs else out[-3000:]


def one(tag):
    dest = corpus.OUT / tag
    if (dest / "hot-result.json").exists():
        return json.loads((dest / "hot-result.json").read_text())
    old = json.loads((corpus.OLD / tag / "result.json").read_text())
    model = old["model"]
    if not old.get("v0", {}).get("builds") or not (dest / "v0.txt").exists():
        return None
    code = (corpus.OLD / tag / "v0.c").read_text()
    report = (dest / "v0.txt").read_text().strip()
    calls = []
    prompt = FIX_HOT.format(report=report, code=code)
    (dest / "hot-prompt.md").write_text(prompt)
    r = ask(model, prompt, dest)
    calls.append(r)
    (dest / "hot-reply.md").write_text(r["result"])
    new = extract_c(r["result"])
    res = dict(tag=tag, model=model)
    if not new:
        res["error"] = "no code block"
    else:
        rounds = 0
        while True:
            ok, errs = try_build(new, tag)
            if ok or rounds == 3:
                break
            rounds += 1
            (dest / f"hot-build-errors-{rounds}.txt").write_text(errs)
            r = ask(model, FIX_BUILD.format(errors=errs, code=new), dest)
            calls.append(r)
            fixed = extract_c(r["result"])
            if not fixed:
                break
            new = fixed
        (dest / "hot.c").write_text(new)
        res.update(builds=ok, build_rounds=rounds)
    res["calls"] = len(calls)
    res["cost_usd"] = round(sum(c.get("total_cost_usd", 0) for c in calls), 4)
    res["claude_wall_s"] = round(sum(c["wall_s"] for c in calls), 1)
    res["output_tokens"] = sum(c.get("usage", {}).get("output_tokens", 0) for c in calls)
    (dest / "hot-result.json").write_text(json.dumps(res, indent=1))
    if res.get("builds"):
        corpus.one(tag, "hot", force=True)
    print(time.strftime("%H:%M:%S"), tag, res, flush=True)
    return res


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--workers", type=int, default=3)
    ap.add_argument("--only")
    a = ap.parse_args()
    tags = sorted(p.name for p in corpus.OLD.iterdir() if (p / "result.json").exists())
    if a.only:
        tags = [t for t in tags if t in a.only.split(",")]
    with ThreadPoolExecutor(a.workers) as ex:
        list(ex.map(one, tags))


if __name__ == "__main__":
    main()
