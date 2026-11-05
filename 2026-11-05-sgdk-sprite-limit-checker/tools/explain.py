"""Turns mdsprites' CSVs into a short report for a human (or a model).

usage: explain.py frames.csv events.csv [--from N]
"""
import argparse
import collections
import csv
import statistics

ap = argparse.ArgumentParser()
ap.add_argument("frames")
ap.add_argument("events")
ap.add_argument("--from", dest="start", type=int, default=0)
a = ap.parse_args()
rows = [r for r in csv.DictReader(open(a.frames)) if int(r["frame"]) >= a.start]
events = [e for e in csv.DictReader(open(a.events)) if int(e["frame"]) >= a.start]
n = len(rows)
hit = [r for r in rows if int(r["sprites_lost"])]
print(f"{n} frames checked. {len(hit)} ({100 * len(hit) / n:.1f}%) lost sprite pixels to a per-line limit.")
print(f"Most sprites on one line: {max(int(r['max_count']) for r in rows)} (limit 20 in H40, 16 in H32). "
      f"Widest line: {max(int(r['max_dots']) for r in rows)} pixels of sprites (limit 320 in H40, 256 in H32). "
      f"Largest sprite list: {max(int(r['chain']) for r in rows)} entries (limit 80 in H40, 64 in H32).")
if hit:
    print(f"In those frames: median {statistics.median(int(r['sprites_lost']) for r in hit):g} sprites lost, "
          f"worst {max(int(r['sprites_lost']) for r in hit)}; median {statistics.median(int(r['loss_lines']) for r in hit):g} "
          f"lines affected, worst {max(int(r['loss_lines']) for r in hit)}.")
    reasons = collections.Counter(e["reason"] for e in events)
    print("Why:", ", ".join(f"{k} {v}" for k, v in reasons.most_common()))
    off = sum(1 for r in hit if int(r["off_slots_on_loss_lines"]))
    if off:
        print(f"In {off} of those frames, off-screen sprites were using up slots on the lines that lost pixels. "
              "They count against both limits; hide them.")
    flag = sum(1 for r in hit if int(r["flag"]))
    print(f"The VDP overflow flag was set in {flag} of those {len(hit)} frames (it does not see the dot limit).")
    lines = collections.Counter()
    for e in events:
        lines[int(e["first_line"]) // 8 * 8] += 1
    print("Screen lines where the losses start:", ", ".join(f"{k}-{k + 7}: {v}" for k, v in sorted(lines.items())[:12]))
    worst = max(hit, key=lambda r: int(r["sprites_lost"]))
    print(f"Worst frame {worst['frame']}:")
    for e in [e for e in events if e["frame"] == worst["frame"]][:12]:
        print(f"  table entry {e['sat']}: {e['w']}x{e['h']} at x={e['x']} y={e['y']}, tile {e['tile']}, "
              f"{e['lines']} lines from line {e['first_line']}, {e['reason']}")
late = sum(1 for r in rows if int(r["late_sat_writes"]))
if late:
    print(f"Note: the sprite table was written during active display in {late} frames; results there are estimates.")
