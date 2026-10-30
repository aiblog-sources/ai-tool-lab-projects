#!/usr/bin/env python3
"""Time pixcheck on every v0 image of the AI test (one process, so Python start-up is not counted). MIT license."""
import json
import statistics
import sys
import time
from pathlib import Path

HERE = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(HERE / "pixcheck"))
import pixcheck  # noqa: E402

SPRITE = {"nes": 16, "sms": 16, "snes": 32, "md": 32}
times = []
for r in json.loads((HERE / "results" / "ai-test.json").read_text()):
    p = HERE / "ai-test" / r["tag"] / "v0" / "art.png"
    if not p.exists():
        continue
    t0 = time.perf_counter()
    rep, _, _ = pixcheck.check(r["console"], str(p), (SPRITE[r["console"]],) * 2 if r["task"] == "knight" else None)
    times.append((time.perf_counter() - t0, r["tag"], rep["info"].get("packing")))
times.sort()
print(f"{len(times)} images: median {statistics.median(t for t, _, _ in times):.2f} s, "
      f"slowest {times[-1][0]:.1f} s ({times[-1][1]}, packing={times[-1][2]}), "
      f"second {times[-2][0]:.1f} s ({times[-2][1]})")
