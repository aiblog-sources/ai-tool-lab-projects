"""Do Claude's NES scenes fit 8 sprites per line, and can it fix them from the report? (MIT)

For each scene x prompt (plain, warned):
  1. ask `claude -p` (tools off, no CLAUDE.md, no settings) for a complete cc65 main.c on the nes/ API
  2. build it; on errors send them back (up to 3 rounds)
  3. run it headless in nessprites (scripted pad, limit off then on)
  4. if it lost sprite pixels, send the nessprites report back once and check the new version
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
OUT = HERE / "ai-test"
SCRATCH = HERE / ".scratch"
ENV = dict(os.environ, CLAUDE_CODE_DISABLE_CLAUDE_MDS="1", CLAUDE_CODE_DISABLE_AUTO_MEMORY="1")
SYSTEM = "You write C code for the NES (Nintendo Entertainment System) using the cc65 compiler."
lock = threading.Lock()
FRAMES, FROM = 1260, 60
INPUTS = ["70-74:S", "100-400:R", "400-700:L", "700-800:U", "800-900:D", "120-124:A", "200-204:A", "260-300:A",
          "500-504:A", "600-640:A", "900-1000:B", "1050-1150:A", "1100-1250:R"]

SCENES = {
    "formation": "a shoot-em-up scene: two rows of 10 enemy ships, each 16x16 pixels, marching left and right "
                 "together as a formation; a player ship (16x16) moves along the bottom with the d-pad and fires "
                 "bullets (8x8) upward with A, up to 6 bullets on screen.",
    "brawler": "a beat-em-up scene: 5 enemies, each 16x32 pixels, walk back and forth along the same ground line "
               "at different speeds; the player (also 16x32) walks with the d-pad.",
    "convoy": "a road: a convoy of 10 cars, each 16x8 pixels, all on the same row of the screen, drives left to "
              "right and wraps around when a car leaves the screen; the player (16x16) moves with the d-pad.",
    "boss": "a boss fight: a 48x48 pixel boss swaying left and right near the top, with 8 fireballs (8x8) that "
            "orbit it in a circle; the player (16x16) moves with the d-pad along the bottom.",
    "rain": "rain: 40 raindrops (8x8) fall at different speeds and restart at a random position at the top; an "
            "umbrella (32x16) moves with the d-pad at the bottom.",
    "snake": "a snake made of 20 segments (8x8) that follows the player's head around the screen with the d-pad, "
             "each segment 4 frames behind the previous one; it starts moving right along the middle row.",
}
PROMPT = """Write a complete NES program in one C file, main.c, for cc65: {task}

You build on this small library, which is already written. Do not write a startup file, linker config, NMI \
handler or CHR data; use what it gives you. The NMI copies the OAM shadow at $0200 to the PPU with OAM DMA once \
you call oam_ready(), and writes ppu_ctrl and ppu_mask to the PPU. Mapper 0, 256x240 screen, controls on pad 1. \
The background is blank (all tile 0); you only draw sprites. Sprite tiles available in the CHR ROM (pattern table \
0, same copy in table 1):

$00 blank, $01 solid colour 1, $02 solid colour 2, $03 solid colour 3, $04 ball, $05 ring, $06 ship, $07 bullet \
(4x4 in the middle of the tile), $08 small dot, $09 star, $0A box outline, $0B diamond, $10/$11 an 8x16 pair \
(body top, body bottom), $12/$13 an 8x16 pair (ball top, ball bottom). Every other tile is blank. Build bigger \
objects from several sprites.

nes.h:
```c
{api}
```

The program must call ppu_setup() with a 32-byte palette first, then loop: update the game, fill OAM, \
oam_ready(), wait_frame().{extra} Reply with the full main.c in one ```c code block."""
WARNED = " It must look right on a real console, with no sprites dropping out or flickering."
FIX_BUILD = """This main.c does not compile with cc65 (cl65 -t nes -Oirs, with nes.h and nes.c from the library). \
Compiler output:

{errors}

Fix it and reply with the full corrected main.c in one ```c code block.

{code}"""
FIX_REPORT = """Here is an NES scene written in C for cc65, and the report of a checker that ran it and evaluated OAM \
for every line of every frame the way the PPU does. Fix the sprite losses without changing what the scene shows: \
same objects, same sizes, same behavior. Reply with the full main.c in one ```c code block.

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
    p = subprocess.run([str(HERE / "nes/build_rom.sh"), str(d / "rom.nes"), str(d / "main.c")],
                       capture_output=True, text=True, timeout=300)
    out = p.stdout + p.stderr
    ok = p.returncode == 0 and (d / "rom.nes").exists()
    errs = [l for l in out.splitlines() if re.search(r"error|unresolved|undefined", l, re.I)]
    return ok, "\n".join(errs[:40]) if errs else out[-3000:], d


def check(d, dest, stage):
    rel = d.relative_to(HERE)
    args = [str(HERE / "tools/rom_check.sh"), str(rel), str(FRAMES), "--shot", f"600:{rel}/shot.ppm"]
    for i in INPUTS:
        args += ["--input", i]
    p = subprocess.run(args, capture_output=True, text=True, timeout=600)
    (dest / f"{stage}-report.txt").write_text(p.stdout + p.stderr)
    if p.returncode or not (d / "frames.csv").exists():
        return dict(error=(p.stdout + p.stderr)[-400:]), ""
    ex = subprocess.run([sys.executable, str(HERE / "tools/explain.py"), str(d / "frames.csv"), str(d / "events.csv"),
                         "--from", str(FROM)], capture_output=True, text=True).stdout
    (dest / f"{stage}-explain.txt").write_text(ex)
    shutil.copy(d / "frames.csv", dest / f"{stage}-frames.csv")
    shutil.copy(d / "events.csv", dest / f"{stage}-events.csv")
    subprocess.run([sys.executable, str(HERE / "tools/ppm2png.py"), str(d / "shot.ppm"), "--out", str(dest)])
    if (dest / "shot.png").exists():
        (dest / "shot.png").rename(dest / f"{stage}-shot.png")
    rows = [r for r in csv.DictReader(open(d / "frames.csv")) if int(r["frame"]) >= FROM]
    hit = [r for r in rows if int(r["sprites_lost"])]
    m = re.search(r"Longest run of consecutive frames one object stayed invisible: (\d+)", ex)
    return dict(frames=len(rows), frames_lost=len(hit), pct_lost=round(100 * len(hit) / len(rows), 1),
                max_sprites_lost=max([int(r["sprites_lost"]) for r in rows] + [0]),
                max_on_line=max(int(r["max_on_line"]) for r in rows),
                longest_invisible=int(m.group(1)) if m else 0,
                real_lines=sum(int(r["real_lines"]) for r in rows), real_pixels=sum(int(r["real_pixels"]) for r in rows),
                real_only=sum(int(r["fn"]) for r in rows), predicted_only=sum(int(r["fp"]) for r in rows),
                flag_wrong=sum(1 for r in rows if r["flag_hw"] != r["flag_exact"]),
                clear_rows=sum(int(r["clear_lost_rows"]) for r in rows),
                lag=(int(rows[-1]["lag"]) - int(rows[0]["lag"])) % 256,
                nmis=(int(rows[-1]["nmi"]) - int(rows[0]["nmi"])) % 256), ex


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
    api = (HERE / "nes/nes.h").read_text().strip()
    r = ask(model, PROMPT.format(task=SCENES[scene], extra=extra, api=api), dest)
    cost.append(r)
    code = extract_c(r["result"]) or ""
    code, v0, ex = evaluate(code, rid, dest, "v0", model, cost)
    res = dict(id=rid, scene=scene, variant=variant, model=model, v0=v0)
    log(rid, "v0", {k: v0.get(k) for k in ("builds", "build_rounds", "pct_lost", "max_sprites_lost", "longest_invisible", "lag", "error")})
    if v0.get("frames_lost"):
        r = ask(model, FIX_REPORT.format(report=ex, code=code), dest)
        cost.append(r)
        new = extract_c(r["result"])
        if new:
            _, v1, _ = evaluate(new, rid, dest, "v1", model, cost)
            res["v1"] = v1
            log(rid, "v1", {k: v1.get(k) for k in ("builds", "pct_lost", "max_sprites_lost", "longest_invisible", "lag", "error")})
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
    ap.add_argument("--only", help="comma-separated scene names")
    a = ap.parse_args()
    OUT.mkdir(exist_ok=True)
    scenes = a.only.split(",") if a.only else list(SCENES)
    jobs = [(s, v, a.model) for s in scenes for v in ("plain", "warned")]
    with ThreadPoolExecutor(a.workers) as ex:
        results = list(ex.map(run_one, jobs))
    if not a.only:
        (OUT / "summary.json").write_text(json.dumps(results, indent=1))
    log("total USD", round(sum(r.get("cost_usd", 0) for r in results), 2))


main()
