"""Run the 19 Claude-written NES scenes of 2026-11-13 through nesflicker (MIT).

Each scene's main.c (v0 = first answer, v1 = after the nessprites report) is
copied from ../2026-11-13-nes-sprite-limit-checker/ai-test/, rebuilt on the same
nes/ runtime, and run with the same scripted pad and frames as that test. No
new model calls. Results land in claude-scenes/<id>/.

usage: claude_scenes.py [--only id,id]
"""
import argparse
import json
import shutil
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent.parent
SRC = HERE.parent / "2026-11-13-nes-sprite-limit-checker" / "ai-test"
OUT = HERE / "claude-scenes"
FRAMES, FROM = 1260, 60
INPUTS = ["70-74:S", "100-400:R", "400-700:L", "700-800:U", "800-900:D", "120-124:A", "200-204:A", "260-300:A",
          "500-504:A", "600-640:A", "900-1000:B", "1050-1150:A", "1100-1250:R"]   # same as the 11-13 ai_test.py


def run(src, dest, stage):
    work = HERE / ".scratch" / f"{dest.name}-{stage}"
    if work.exists():
        shutil.rmtree(work)
    work.mkdir(parents=True)
    shutil.copy(src, work / "main.c")
    shutil.copy(src, dest / f"{stage}.c")
    subprocess.run([str(HERE / "nes/build_rom.sh"), str(work / "rom.nes"), str(work / "main.c")], check=True,
                   capture_output=True)
    rel = work.relative_to(HERE)
    args = [str(HERE / "tools/rom_check.sh"), str(rel), str(FRAMES), "--shot", f"600:{rel}/shot.ppm"]
    for i in INPUTS:
        args += ["--input", i]
    p = subprocess.run(args, capture_output=True, text=True, check=True)
    (dest / f"{stage}-nesflicker.txt").write_text(p.stdout)
    flick = [sys.executable, str(HERE / "tools/flicker.py"), str(work / "oam.csv"), "--from", str(FROM)]
    rep = subprocess.run(flick + ["--report"], capture_output=True, text=True, check=True).stdout
    (dest / f"{stage}-flicker.txt").write_text(rep)
    subprocess.run(flick + ["--json", str(dest / f"{stage}-flicker.json"), "--tracks", str(work / "tracks.csv")],
                   capture_output=True, check=True)
    shutil.copy(work / "frames.csv", dest / f"{stage}-frames.csv")
    subprocess.run(["gzip", "-9", "-c", str(work / "oam.csv")], stdout=open(dest / f"{stage}-oam.csv.gz", "wb"), check=True)
    subprocess.run([sys.executable, str(HERE / "tools/ppm2png.py"), str(work / "shot.ppm"), "--out", str(dest)], check=True)
    if (dest / "shot.png").exists():
        (dest / "shot.png").rename(dest / f"{stage}-shot.png")
    return json.loads((dest / f"{stage}-flicker.json").read_text())["summary"]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--only")
    a = ap.parse_args()
    ids = sorted(p.name for p in SRC.iterdir() if p.is_dir())
    if a.only:
        ids = [i for i in ids if i in a.only.split(",")]
    OUT.mkdir(exist_ok=True)
    allres = {}
    for rid in ids:
        dest = OUT / rid
        dest.mkdir(exist_ok=True)
        for stage in ("v0", "v1"):
            src = SRC / rid / f"{stage}.c"
            if not src.exists():
                continue
            s = run(src, dest, stage)
            allres[f"{rid}/{stage}"] = s
            print(rid, stage, json.dumps(s), flush=True)
    if not a.only:
        (OUT / "summary.json").write_text(json.dumps(allres, indent=1))


if __name__ == "__main__":
    main()
