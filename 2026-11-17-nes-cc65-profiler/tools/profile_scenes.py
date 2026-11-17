#!/usr/bin/env python3
"""Profile the 19 Claude-written NES scenes with nesprof (MIT).

The scenes come from ../2026-11-15-nes-flicker-checker/claude-scenes/<id>/v*.c
(written by Claude Sonnet 5.5 on 2026-11-13; v1 = the fix after the sprite
report). Each is rebuilt on the same nes/ runtime with debug info, checked to
be byte-identical to a build without -g, and run with the same pad script and
frames as those tests. No model calls. Output: scenes/<id>/<v>-*.txt|csv and
results/scenes-summary.json + results/scenes-table.md.

With --castfix the scenes are built on nes-castfix/ instead (the same
library with the OAM_*() offsets cast to a byte), written to scenes-castfix/
and results/castfix-*, and each is checked for the same picture as the
original build (same_picture.py, no input, 700 frames).

usage: profile_scenes.py [--only id,id] [--castfix]
"""
import argparse

import csv
import json
import shutil
import subprocess
from pathlib import Path

import same_picture

HERE = Path(__file__).resolve().parent.parent
SRC = HERE.parent / "2026-11-15-nes-flicker-checker" / "claude-scenes"
OLD_BUILD = HERE.parent / "2026-11-15-nes-flicker-checker" / "nes" / "build_rom.sh"
OUT = HERE / "scenes"
FRAMES, FROM = 1260, 60
INPUTS = ["70-74:S", "100-400:R", "400-700:L", "700-800:U", "800-900:D", "120-124:A", "200-204:A", "260-300:A",
          "500-504:A", "600-640:A", "900-1000:B", "1050-1150:A", "1100-1250:R"]   # same as the 11-13 ai_test.py
NOT_RUNTIME = {"nmi", "reset", "irq", "[unmapped]"}


def profile(rom, dest, tag, extra=()):
    rel = lambda p: str(Path(p).relative_to(HERE))
    args = [str(HERE / "checker/nesprof.sh"), rel(rom), str(FRAMES), "--from", str(FROM),
            "--funcs", rel(dest / f"{tag}-funcs.csv"), "--loops", rel(dest / f"{tag}-loops.csv"),
            "--csv", rel(dest / f"{tag}-frames.csv"), "--pcs", rel(dest / f"{tag}-pcs.csv"), "--spikes", "5", rel(dest / f"{tag}-spikes.csv"),
            "--ram", "07F6:lag",
            *extra]
    for i in INPUTS:
        args += ["--input", i]
    p = subprocess.run(args, capture_output=True, text=True, check=True, cwd=HERE)
    (dest / f"{tag}-nesprof.txt").write_text(p.stdout)
    subprocess.run(["python3", str(HERE / "tools/lines.py"), str(rom.with_suffix(".dbg")), str(dest / f"{tag}-pcs.csv"),
                    str(FRAMES - FROM), "--src", str(dest), "--top", "15", "--md", str(dest / f"{tag}-lines.md")],
                   check=True, capture_output=True)
    subprocess.run(["python3", str(HERE / "tools/lines.py"), str(rom.with_suffix(".dbg")), str(dest / f"{tag}-spikes.csv"),
                    "0", "--spikes", "--src", str(dest), "--top", "3", "--md", str(dest / f"{tag}-spikes.md")],
                   check=True, capture_output=True)
    return summarize(dest, tag)


def summarize(dest, tag):
    loops = list(csv.DictReader(open(dest / f"{tag}-loops.csv")))[1:]   # the first loop starts mid-run
    funcs = list(csv.DictReader(open(dest / f"{tag}-funcs.csv")))
    frames = list(csv.DictReader(open(dest / f"{tag}-frames.csv")))
    busy_total = sum(int(r["busy"]) for r in frames)
    work = [f for f in funcs if f["function"] != "_wait_frame"]
    top = max(work, key=lambda f: int(f["self"]))
    incl = sorted((f for f in work if f["function"].startswith("_") and f["function"] != "_main"),
                  key=lambda f: -int(f["inclusive"]))
    runtime = [f for f in work if not f["function"].startswith("_") and f["function"] not in NOT_RUNTIME]
    rt = sum(int(f["self"]) for f in runtime)
    rt_top = sorted(runtime, key=lambda f: -int(f["self"]))[:3]
    lag = [int(r["lag"]) for r in frames]
    return {
        "loops": len(loops),
        "slow_loops": sum(1 for r in loops if int(r["nmis"]) >= 2),
        "busy_per_loop_avg": round(sum(int(r["busy"]) for r in loops) / max(1, len(loops))),
        "busy_per_loop_max": max(int(r["busy"]) for r in loops),
        "busy_share": round(busy_total / sum(int(r["cycles"]) for r in frames), 4),
        "top_self": [top["function"], round(int(top["self"]) / busy_total, 4)],
        "top_inclusive": [[f["function"], round(int(f["inclusive"]) / busy_total, 4)] for f in incl[:3]],
        "runtime_share": round(rt / busy_total, 4),
        "runtime_top": [[f["function"], round(int(f["self"]) / busy_total, 4), int(f["calls"])] for f in rt_top],
        "rom_lag_frames": (lag[-1] - lag[0]) % 256,
    }


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--only")
    ap.add_argument("--castfix", action="store_true")
    a = ap.parse_args()
    global OUT
    nes_dir, prefix = ("nes-castfix", "castfix-") if a.castfix else ("nes", "")
    if a.castfix:
        OUT = HERE / "scenes-castfix"
    only = set(a.only.split(",")) if a.only else None
    summary = {}
    for sdir in sorted(SRC.iterdir()):
        if only and sdir.name not in only:
            continue
        dest = OUT / sdir.name
        dest.mkdir(parents=True, exist_ok=True)
        for src in sorted(sdir.glob("v*.c")):
            tag = src.stem
            shutil.copy(src, dest / f"{tag}.c")
            rom = HERE / ".scratch" / (prefix + sdir.name) / f"{tag}.nes"
            subprocess.run([str(HERE / nes_dir / "build_rom.sh"), str(rom), str(src)], check=True, capture_output=True)
            if a.castfix:
                s = profile(rom, dest, tag)
                orig = HERE / ".scratch" / sdir.name / f"{tag}.nes"
                for r, o in ((orig, rom.with_name(f"{tag}-orig-oam.csv")), (rom, rom.with_name(f"{tag}-oam.csv"))):
                    subprocess.run([str(HERE / "checker/nesprof.sh"), str(r.relative_to(HERE)), "700", "--oam",
                                    str(o.relative_to(HERE))], check=True, capture_output=True, cwd=HERE)
                s["picture"] = same_picture.compare(rom.with_name(f"{tag}-orig-oam.csv"), rom.with_name(f"{tag}-oam.csv"))
            else:
                plain = rom.with_name(f"{tag}-plain.nes")
                subprocess.run([str(OLD_BUILD), str(plain), str(src)], check=True, capture_output=True)
                same = rom.read_bytes() == plain.read_bytes()
                s = profile(rom, dest, tag)
                s["same_bytes_as_plain_build"] = same
            summary[f"{sdir.name}/{tag}"] = s
            print(sdir.name, tag, json.dumps(s), flush=True)
    out = HERE / "results" / (prefix + ("scenes-summary.json" if not only else "scenes-summary-partial.json"))
    out.write_text(json.dumps(summary, indent=1) + "\n")
    if not only:
        with open(HERE / "results" / (prefix + "scenes-table.md"), "w") as f:
            f.write("| scene | ver | slow loops | busy/loop | hottest (self) | cc65 runtime |\n| --- | --- | --- | --- | --- | --- |\n")
            for k, s in summary.items():
                sc, v = k.split("/")
                f.write(f"| {sc} | {v} | {s['slow_loops']}/{s['loops']} | {s['busy_per_loop_avg']:,} | "
                        f"`{s['top_self'][0]}` {s['top_self'][1]:.0%} | {s['runtime_share']:.0%} |\n")


if __name__ == "__main__":
    main()
