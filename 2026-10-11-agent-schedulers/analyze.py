"""Summarizes a timing run: how late each scheduler fired, and what the long-running daemons cost.

Usage: python3 analyze.py <results-dir> [start_epoch end_epoch]
Reads stamps.log ("<scheduler> <epoch>") and resources.tsv (from sample.sh).
"""
import csv
import statistics
import sys
from collections import defaultdict
from pathlib import Path

results = Path(sys.argv[1])
lo, hi = (float(sys.argv[2]), float(sys.argv[3])) if len(sys.argv) == 4 else (0, float("inf"))

fired = defaultdict(list)
for line in (results / "stamps.log").read_text().splitlines():
    name, ts = line.split()
    ts = float(ts)
    if lo <= ts < hi:
        fired[name].append(ts)

print("| Scheduler | Runs | Minutes covered | Median delay | Max delay |")
print("| --- | --- | --- | --- | --- |")
for name, times in sorted(fired.items()):
    # Every job is scheduled at second 0 of a minute; the delay is how far past that minute it ran.
    delays = [t - (t // 60) * 60 for t in times]
    minutes = {int(t // 60) for t in times}
    span = int(max(times) // 60) - int(min(times) // 60) + 1
    print(f"| {name} | {len(times)} | {len(minutes)} of {span} | {statistics.median(delays):.2f} s | {max(delays):.2f} s |")

rows = []
with open(results / "resources.tsv") as f:
    for r in csv.DictReader(f, delimiter="\t"):
        if r["gateway_rss_kb"] and lo <= float(r["epoch"]) < hi:
            rows.append(r)
if len(rows) >= 2:
    hours = (float(rows[-1]["epoch"]) - float(rows[0]["epoch"])) / 3600
    for proc in ("gateway", "crond"):
        rss = [int(r[f"{proc}_rss_kb"]) / 1024 for r in rows if r[f"{proc}_rss_kb"]]
        cpu = float(rows[-1][f"{proc}_cpu_s"]) - float(rows[0][f"{proc}_cpu_s"])
        print(f"{proc}: RSS median {statistics.median(rss):.1f} MB, max {max(rss):.1f} MB; "
              f"CPU {cpu:.2f} s over {hours * 60:.1f} min = {cpu / hours:.1f} CPU-s per hour")
