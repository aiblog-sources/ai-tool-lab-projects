#!/usr/bin/env python3
"""Run sgdkhot on the 40 Claude-written games of the 2026-10-29 test (every stored version: v0, lint, lint2, plain,
and hot from tools/fix_hot.py), with the same scripted pad and frame count as that test.

Writes results/corpus/<program>/<stage>.json (lines, findings, verdicts) and <stage>.txt (the report).
usage: corpus.py [--stages v0,lint2,plain] [--workers 3] [--only tag,tag]
"""
import argparse
import json
import shutil
import sys
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import sgdkhot  # noqa: E402

HERE = Path(__file__).resolve().parent.parent
OLD = HERE.parent / "2026-10-29-sgdk-perf-lint" / "ai-test"
OUT = HERE / "results" / "corpus"
INPUTS = ["70-74:S", "100-400:R", "400-700:L", "700-800:U", "800-900:D", "120-124:A", "200-204:A",
          "260-300:A", "500-504:A", "600-640:A", "900-1000:B", "1000-1004:C", "1050-1150:A", "1100-1250:R",
          "300-304:S"]  # the 2026-10-29 pad script
FRAMES, FROM = 1260, 60


def source(tag, stage):
    if stage == "hot":
        p = OUT / tag / "hot.c"
    else:
        p = OLD / tag / f"{stage}.c"
    return p if p.exists() else None


def one(tag, stage, force=False):
    dest = OUT / tag
    dest.mkdir(parents=True, exist_ok=True)
    if (dest / f"{stage}.json").exists() and not force:
        return json.loads((dest / f"{stage}.json").read_text())
    src = source(tag, stage)
    if not src:
        return None
    proj = sgdkhot.WORK / "hot" / f"{tag}-{stage}"
    if proj.exists():
        shutil.rmtree(proj)
    (proj / "src").mkdir(parents=True)
    shutil.copy(src, proj / "src" / "main.c")
    try:
        sgdkhot.build(proj)
    except SystemExit as e:
        res = dict(tag=tag, stage=stage, builds=False, error=str(e)[-2000:])
        (dest / f"{stage}.json").write_text(json.dumps(res, indent=1))
        return res
    sgdkhot.profile(proj, FRAMES, FROM, INPUTS, extra=["--shot", "600:/p/out/shot.ppm"])
    meta, rows = sgdkhot.read_lines(proj / "out" / "lines.csv")
    findings, _ = sgdkhot.run_lint(proj, sgdkhot.LINT)
    a = sgdkhot.analyse(proj, meta, rows, findings)
    prof = json.loads((proj / "out" / "prof.json").read_text())
    busy_csv = 0
    for line in (proj / "out" / "frames.csv").read_text().splitlines()[1:]:
        c = line.split(",")
        if int(c[0]) >= FROM:
            busy_csv += int(c[2])
    a.update(tag=tag, stage=stage, builds=True, mean_load=prof["mean_load_pct"], dropped=prof["dropped_frames"],
             busy_check=dict(frames_csv=busy_csv, charged=round(a["busy_per_frame"] * a["frames"])))
    (dest / f"{stage}.txt").write_text(sgdkhot.report(a, proj / "src", 15) + "\n")
    if (proj / "out" / "shot.ppm").exists():
        shutil.copy(proj / "out" / "shot.ppm", dest / f"{stage}-shot.ppm")
    (dest / f"{stage}.json").write_text(json.dumps(a, indent=1, default=str))
    print(tag, stage, f"load {a['mean_load']} drop {a['dropped']} findings {len(findings)}", flush=True)
    return a


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--stages", default="v0,lint2,plain")
    ap.add_argument("--workers", type=int, default=3)
    ap.add_argument("--only")
    ap.add_argument("--force", action="store_true")
    a = ap.parse_args()
    tags = sorted(p.name for p in OLD.iterdir() if (p / "result.json").exists())
    if a.only:
        tags = [t for t in tags if t in a.only.split(",")]
    jobs = [(t, s) for t in tags for s in a.stages.split(",")]
    with ThreadPoolExecutor(a.workers) as ex:
        list(ex.map(lambda j: one(*j, force=a.force), jobs))


if __name__ == "__main__":
    main()
