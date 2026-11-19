"""Does Claude's devkitSMS code write VRAM too fast during active display? (MIT)
For each scene x prompt (plain, warned):
  1. ask `claude -p` (tools off, no CLAUDE.md, no settings) for a complete main.c on SMSlib, with SMSlib.h
     in the prompt
  2. build it with SDCC + SMSlib (sms/build-rom.sh); on errors send them back (up to 3 rounds)
  3. run it headless in smsvdp for 600 frames (no input)
  4. if smsvdp found writes under 26 cycles during active display, send the report back once and check again
Everything lands in ai-test/<id>/. usage: ai_test.py [--model claude-sonnet-5-5] [--workers 3]
"""
import argparse
import csv
import json
import os
import re
import shutil
import subprocess
import threading
import time
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

HERE = Path(__file__).resolve().parent.parent
OUT = HERE / "ai-test"
WORK = Path(os.environ.get("WORK", Path.home() / ".cache/smsvdp"))
SCRATCH = WORK / "ai"
ENV = dict(os.environ, CLAUDE_CODE_DISABLE_CLAUDE_MDS="1", CLAUDE_CODE_DISABLE_AUTO_MEMORY="1")
SYSTEM = "You write C code for the Sega Master System using devkitSMS (SMSlib) and the SDCC compiler."
lock = threading.Lock()
FRAMES, FROM, SHOT = 600, 60, 300

SCENES = {
    "hud": "a score display: the top row of the screen shows SCORE and a 6-digit score drawn with background tiles "
           "for the digits; the score goes up by 10 every frame. Below it, 8 enemy sprites (8x8) move left and "
           "right across the screen.",
    "water": "an animated background: the whole screen is filled with a water tile, and the water animates by "
             "replacing that tile's graphics with the next of 4 animation frames every 8 frames. A boat sprite "
             "(16x16, made of 4 sprites) bobs up and down in the middle.",
    "scroller": "a horizontally scrolling level 64 tiles wide (wider than the 32-column tile map): the screen "
                "scrolls one pixel per frame to the right, and each new column of the level is written into the "
                "tile map just before it comes into view. Make the level from a few tiles (ground, bricks, sky).",
    "fade": "a palette fade: a picture made of background tiles in 8 colors fades in from black to full color "
            "and back out to black, one brightness step every 4 frames, repeating forever. 16 star sprites twinkle "
            "by changing their tile.",
    "dialog": "a dialog box: a framed box on the bottom 6 rows of the screen types out a few lines of text, one "
              "character every frame, then clears the box and starts again with the next message. A character "
              "sprite (16x16) walks back and forth above the box.",
    "bullets": "a shoot-em-up: 40 bullets (8x8 sprites) fly out from the center of the screen in a spiral and "
               "wrap around the edges, and the background has a star field of 64 stars drawn as background tiles "
               "that move down one tile row every 8 frames.",
    "boss": "a big boss drawn with background tiles: a 48x32 pixel boss (6x4 = 24 tiles) in the middle of the "
            "screen whose graphics animate through 4 frames, a new frame every frame (60 animation frames per "
            "second), by replacing the graphics of its 24 tiles. A player sprite (16x16) moves left and right "
            "below it.",
    "life": "Conway's Game of Life on the whole 32x24 tile map: every cell is one background tile (alive or dead), "
            "the simulation advances one generation every frame and the whole tile map is redrawn every frame. "
            "Start from a random pattern.",
}

PROMPT = """Write a complete Sega Master System program in one C file, main.c, using devkitSMS (SMSlib) and SDCC: \
{task}
Define every tile and palette in main.c as C arrays (no external assets). The program is built with \
`sdcc -c -mz80 main.c`, linked with devkitSMS's crt0_sms.rel and SMSlib.lib, and made into a ROM with makesms. \
Put SMS_EMBED_SEGA_ROM_HEADER(9999, 0); at the end of the file.{extra} This is SMSlib.h:
```c
{api}
```
Reply with the full main.c in one ```c code block."""
WARNED = " It must work correctly on a real Master System console, not only in emulators."
FIX_BUILD = """This main.c does not build with SDCC and SMSlib (sdcc -c -mz80, linked with crt0_sms.rel and \
SMSlib.lib). Output:
{errors}
Fix it and reply with the full corrected main.c in one ```c code block.
{code}"""
FIX_REPORT = """Here is a Sega Master System program written in C with devkitSMS, and the report of a checker that \
ran it in an emulator and timed every write to the VDP data port. On real hardware, while the display is \
active (lines 0-191 with the screen on), writes that come too close together are lost or corrupted; the checker \
counts writes less than 26 CPU cycles after the previous one (23 and 29 are shown too, as other published \
figures). It also counts visible lines drawn with the display switched off, which show as blank lines on screen. \
Fix the program so it has no writes under 26 cycles during active display and no blank visible lines, without \
changing what it shows. Reply with the full main.c in one ```c code block.
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
    d = SCRATCH / tag
    if d.exists():
        shutil.rmtree(d)
    d.mkdir(parents=True)
    (d / "main.c").write_text(code)
    p = subprocess.run([str(HERE / "sms/build-rom.sh"), str(d), str(d / "rom.sms")],
                       capture_output=True, text=True, timeout=600)
    out = p.stdout + p.stderr
    ok = p.returncode == 0 and (d / "rom.sms").exists()
    errs = [l for l in out.splitlines() if re.search(r"error|undefined|unresolved", l, re.I)]
    return ok, "\n".join(errs[:40]) if errs else out[-3000:], d


def check(d, dest, stage):
    rel = d.relative_to(WORK)
    args = ["docker", "run", "--rm", "--network", "none", "--user", f"{os.getuid()}:{os.getgid()}",
            "-v", f"{WORK}:/w", "-w", "/w", "gcc:14", "./smsvdp", "gpgx/genesis_plus_gx_libretro.so",
            f"{rel}/rom.sms", str(FRAMES), "--from", str(FROM), "--sym", f"{rel}/rom.noi",
            "--csv", f"{rel}/frames.csv", "--sites", f"{rel}/sites.csv", "--shot", f"{SHOT}:{rel}/shot.ppm"]
    p = subprocess.run(args, capture_output=True, text=True, timeout=600)
    report = p.stdout + p.stderr
    (dest / f"{stage}-report.txt").write_text(report)
    if p.returncode not in (0, 3, 4) or not (d / "frames.csv").exists():
        return dict(error=report[-400:]), report
    shutil.copy(d / "frames.csv", dest / f"{stage}-frames.csv")
    shutil.copy(d / "sites.csv", dest / f"{stage}-sites.csv")
    subprocess.run(["python3", str(HERE / "tools/ppm2png.py"), str(d / "shot.ppm"), "--out", str(dest)])
    if (dest / "shot.png").exists():
        (dest / "shot.png").rename(dest / f"{stage}-shot.png")
    rows = [r for r in csv.DictReader(open(d / "frames.csv")) if int(r["frame"]) >= FROM]
    s = lambda k: sum(int(r[k]) for r in rows)
    return dict(frames=len(rows), writes=s("data_writes"), active=s("active_writes"), lt23=s("fast_lt23"),
                lt26=s("fast_lt26"), lt29=s("fast_lt29"),
                frames_lt26=sum(1 for r in rows if int(r["fast_lt26"])),
                frames_blank=sum(1 for r in rows if int(r["blank_lines"])),
                max_blank=max(int(r["blank_lines"]) for r in rows),
                frames_off=sum(1 for r in rows if int(r["screen_off"])),
                frames_active=sum(1 for r in rows if int(r["active_writes"])),
                uses_unsafe=None), report


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
    info = dict(build_rounds=rounds, builds=ok, uses_unsafe=bool(re.search(r"UNSAFE_", code)),
                raw_port=bool(re.search(r"__sfr|0xBE|0xbe", code)), waits_vblank=code.count("SMS_waitForVBlank"))
    report = ""
    if ok:
        res, report = check(d, dest, stage)
        res.pop("uses_unsafe", None)
        info.update(res)
    return code, info, report


def needs_fix(v):
    return bool(v.get("lt26") or v.get("frames_blank"))


def run_one(job):
    scene, variant, model = job
    rid = f"{scene}-{variant}"
    dest = OUT / rid
    cost = []
    t0 = time.time()
    if (dest / "result.json").exists():
        # re-check the saved first version with the current checker (no model call; the build is
        # deterministic), then run the fix round if it now needs one and has none yet
        res = json.loads((dest / "result.json").read_text())
        code = (dest / "v0.c").read_text()
        _, v0, report = evaluate(code, rid, dest, "v0", model, [])
        v0["build_rounds"] = res["v0"].get("build_rounds", 0)
        res["v0"] = v0
        if "v1" in res and (dest / "v1.c").exists():
            _, v1, _ = evaluate((dest / "v1.c").read_text(), rid, dest, "v1", model, [])
            v1["build_rounds"] = res["v1"].get("build_rounds", 0)
            res["v1"] = v1
        if "v1" in res or not needs_fix(v0):
            (dest / "result.json").write_text(json.dumps(res, indent=1))
            return res
        prior = [dict(total_cost_usd=res.get("cost_usd", 0), usage=dict(input_tokens=res.get("input_tokens", 0),
                      output_tokens=res.get("output_tokens", 0)))] * 1
        cost.extend(prior)
        res_calls = res.get("calls", 0)
    else:
        dest.mkdir(parents=True, exist_ok=True)
        extra = WARNED if variant == "warned" else ""
        api = (WORK / "smslib-build/SMSlib.h").read_text().strip()
        r = ask(model, PROMPT.format(task=SCENES[scene], extra=extra, api=api), dest)
        cost.append(r)
        code = extract_c(r["result"]) or ""
        code, v0, report = evaluate(code, rid, dest, "v0", model, cost)
        res = dict(id=rid, scene=scene, variant=variant, model=model, v0=v0)
        res_calls = 0
        log(rid, "v0", {k: v0.get(k) for k in ("builds", "build_rounds", "active", "lt23", "lt26", "lt29", "frames_blank", "uses_unsafe", "error")})
    if needs_fix(v0):
        r = ask(model, FIX_REPORT.format(report=report, code=code), dest)
        cost.append(r)
        new = extract_c(r["result"])
        if new:
            _, v1, _ = evaluate(new, rid, dest, "v1", model, cost)
            res["v1"] = v1
            log(rid, "v1", {k: v1.get(k) for k in ("builds", "active", "lt23", "lt26", "lt29", "frames_blank", "uses_unsafe", "error")})
    res["cost_usd"] = round(sum(c.get("total_cost_usd", 0) for c in cost), 4)
    res["calls"] = res_calls + len(cost) - (1 if res_calls else 0)
    res["input_tokens"] = sum(c.get("usage", {}).get("input_tokens", 0) for c in cost)
    res["output_tokens"] = sum(c.get("usage", {}).get("output_tokens", 0) for c in cost)
    res["wall_s"] = round(res.get("wall_s", 0) + time.time() - t0, 1)
    (dest / "result.json").write_text(json.dumps(res, indent=1))
    return res


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--model", default="claude-sonnet-5-5")
    ap.add_argument("--workers", type=int, default=3)
    ap.add_argument("--only", help="comma-separated scene names")
    ap.add_argument("--out", default="ai-test", help="results folder (one per model)")
    a = ap.parse_args()
    global OUT
    OUT = HERE / a.out
    OUT.mkdir(exist_ok=True)
    scenes = a.only.split(",") if a.only else list(SCENES)
    jobs = [(s, v, a.model) for s in scenes for v in ("plain", "warned")]
    with ThreadPoolExecutor(a.workers) as ex:
        results = list(ex.map(run_one, jobs))
    if not a.only:
        (OUT / "summary.json").write_text(json.dumps(results, indent=1))
    log("total USD", round(sum(r.get("cost_usd", 0) for r in results), 2))


main()
