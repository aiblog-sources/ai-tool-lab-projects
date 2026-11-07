"""Print --shot arguments for the middle flush frame of some phases (from a first mddma CSV)."""
import csv, sys
path, prefix = sys.argv[1], sys.argv[2]
want = [int(x) for x in sys.argv[3].split(",")]
by = {}
for r in csv.DictReader(open(path)):
    if int(r["flushes"]) > 0:
        by.setdefault(int(r["phase"]), []).append(int(r["frame"]))
print(" ".join(f"--shot {by[p][len(by[p]) // 2]}:{prefix}-p{p}.ppm" for p in want if p in by))
