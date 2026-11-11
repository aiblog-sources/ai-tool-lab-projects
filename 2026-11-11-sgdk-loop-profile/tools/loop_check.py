"""check.sh for the agent loop: build <dir>/src/main.c with SGDK 2.11 and print one kind of mdperf report:
  rate    only the game loops per second (what a stopwatch or an fps counter would tell you)
  loops   the 2026-11-09 report: DMA, CPU by function (busiest video frames), game loop totals
  loopfn  DMA, CPU by function, and game loops by function (added up over every frame a loop spans)
Every call is logged outside the work folder, in $WORK/loop-calls/<name>.jsonl.
usage: loop_check.py <work folder> <rate|loops|loopfn>"""
import json
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import mdperf  # noqa: E402
from sgdk_build import WORK, build  # noqa: E402

FRAMES, FROM = 1500, 700
INPUTS = ["70-74:S", "100-400:R", "400-700:L", "700-800:U", "800-900:D", "120-124:A", "200-204:A", "260-300:A",
          "500-504:A", "600-640:A", "900-1000:B", "1000-1004:C", "1050-1150:A", "1100-1250:R", "1250-1500:L",
          "1300-1304:A", "1400-1440:A"]


def main():
    folder = Path(sys.argv[1]).resolve()
    name = folder.name
    logdir = WORK / "loop-calls"
    logdir.mkdir(parents=True, exist_ok=True)
    n = sum(1 for _ in open(logdir / f"{name}.jsonl")) if (logdir / f"{name}.jsonl").exists() else 0
    code = (folder / "src" / "main.c").read_text()
    ok, errs, d = build(code, f"loop-{name}-{n}")
    rec = dict(call=n, time=time.strftime("%H:%M:%S"), builds=ok)
    if not ok:
        print("BUILD FAILED (SGDK 2.11):\n" + errs)
    else:
        s, texts = mdperf.analyze(d, f"loop-{name}-{n}", FRAMES, FROM, INPUTS)
        print(texts[sys.argv[2]])
        rec.update(s)
    with open(logdir / f"{name}.jsonl", "a") as f:
        f.write(json.dumps(rec) + "\n")


if __name__ == "__main__":
    main()
