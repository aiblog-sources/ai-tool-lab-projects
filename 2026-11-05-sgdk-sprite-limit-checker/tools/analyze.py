"""Per-phase table from the mdsprites CSVs of the test ROM."""
import collections
import csv
import statistics
import sys

SKIP = 5  # frames at the start of a phase, while the table settles
rows = list(csv.DictReader(open(sys.argv[1])))
events = list(csv.DictReader(open(sys.argv[2])))
by_phase = collections.defaultdict(list)
for r in rows:
    by_phase[int(r["phase"])].append(r)
ev_by_frame = collections.defaultdict(list)
for e in events:
    ev_by_frame[int(e["frame"])].append(e)

print("phase frames chain max_n max_dots lost_per_frame loss_lines real_lines real_px flag_frames fp fn busy_median")
for ph in sorted(by_phase):
    rs = by_phase[ph][SKIP:]
    if not rs or ph in (0, 99):
        continue
    med = lambda k: statistics.median(int(r[k]) for r in rs)
    tot = lambda k: sum(int(r[k]) for r in rs)
    print(ph, len(rs), int(med("chain")), int(med("max_count")), int(med("max_dots")), med("sprites_lost"),
          med("loss_lines"), med("real_lines"), med("real_pixels"), tot("flag"), tot("fp"), tot("fn"),
          int(med("busy_cycles")))
    reasons = collections.Counter()
    xs = collections.Counter()
    for r in rs:
        for e in ev_by_frame[int(r["frame"])]:
            reasons[e["reason"]] += 1
            xs[int(e["x"])] += 1
    if reasons:
        print("   reasons:", dict(reasons))
        share = sorted((x, round(100 * n / len(rs), 1)) for x, n in xs.items())
        print("   % of frames each x lost pixels:", share)
print()
print("all frames: predicted and real", sum(int(r["tp"]) for r in rows), "predicted only",
      sum(int(r["fp"]) for r in rows), "real only", sum(int(r["fn"]) for r in rows))
print("frames with late table writes:", sum(1 for r in rows if int(r["late_sat_writes"])))
