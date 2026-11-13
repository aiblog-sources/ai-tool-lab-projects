"""Turns nessprites' CSVs into a short report for a person (or a model). MIT.

usage: explain.py frames.csv events.csv [--from N]

Besides counts it measures how long one object stays invisible. Objects have
no identity in OAM (a flicker routine moves them between entries), so a lost
sprite in frame f+1 counts as "the same object again" if a sprite was lost
within 4 pixels of it in frame f. Long streaks are dropout; streaks of 1 or 2
frames are flicker.
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


def streaks(events):
    by = collections.defaultdict(list)
    for e in events:
        by[int(e["frame"])].append((int(e["x"]), int(e["y"])))
    run = {}  # (x, y) in the previous frame -> streak length
    best, prev_f = 0, None
    for f in sorted(by):
        cur = {}
        for p in by[f]:
            n = 1
            if prev_f == f - 1:
                n += max([v for q, v in run.items() if abs(q[0] - p[0]) <= 4 and abs(q[1] - p[1]) <= 4], default=0)
            cur[p] = n
            best = max(best, n)
        run, prev_f = cur, f
    return best


def report(rows, events):
    n = len(rows)
    hit = [r for r in rows if int(r["sprites_lost"])]
    out = [f"{n} frames checked. {len(hit)} ({100 * len(hit) / n:.1f}%) lost visible sprite pixels to the 8 sprites per line limit."]
    out.append(f"Most sprites in range on one line: {max(int(r['max_on_line']) for r in rows)} (the PPU draws the first 8 in OAM order).")
    if hit:
        out.append(f"In those frames: median {statistics.median(int(r['sprites_lost']) for r in hit):g} sprites lost, worst "
                   f"{max(int(r['sprites_lost']) for r in hit)}; median {statistics.median(int(r['loss_lines']) for r in hit):g} "
                   f"lines affected, worst {max(int(r['loss_lines']) for r in hit)}.")
        s = streaks(events)
        kind = "dropout (the same objects stay missing)" if s > 4 else "flicker (lost objects change from frame to frame)"
        out.append(f"Longest run of consecutive frames one object stayed invisible: {s} ({kind}).")
        lines = collections.Counter()
        for e in events:
            lines[int(e["first_line"]) // 8 * 8] += 1
        out.append("Screen lines where the losses start: " + ", ".join(f"{k}-{k + 7}: {v}" for k, v in sorted(lines.items())[:12]))
        worst = max(hit, key=lambda r: int(r["sprites_lost"]))
        out.append(f"Worst frame {worst['frame']}:")
        for e in [e for e in events if e["frame"] == worst["frame"]][:12]:
            out.append(f"  OAM entry {e['oam']}: tile {e['tile']} at x={e['x']} y={e['y']}, {e['lines']} lines lost from line {e['first_line']}")
    flag_wrong = sum(1 for r in rows if r["flag_hw"] != r["flag_exact"])
    if flag_wrong:
        out.append(f"The real PPU's sprite overflow flag would be wrong in {flag_wrong} frames (its hardware bug), "
                   "so do not use it to detect this.")
    clear = sum(int(r["clear_lost_rows"]) for r in rows)
    if clear:
        out.append(f"Also {clear} lost sprite rows were transparent anyway (they still took one of the 8 slots).")
    if "lag" in rows[0]:
        lag = (int(rows[-1]["lag"]) - int(rows[0]["lag"])) % 256
        out.append(f"Lag frames (the program had not finished its OAM table by the NMI): {lag} of {n}.")
    return "\n".join(out)


print(report(rows, events))
