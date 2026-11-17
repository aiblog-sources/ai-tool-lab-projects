#!/usr/bin/env python3
"""costs.py costrom-funcs.csv [--md out.md]: cycles per item for each way in costrom/main.c (MIT).

Each fill_* / arithmetic function handles 64 items per call, so the cost
per item is inclusive cycles / calls / 64 (loop overhead included). The
runtime helpers each one called are listed from a second run's per-function
self cycles (not needed here: the inclusive figure already holds them)."""
import csv
import sys

ORDER = ["_fill_int_args", "_fill_u8_args", "_fill_macros_inline", "_fill_macros_cast", "_fill_byte_offset", "_fill_static_vars",
         "_copy_u8", "_and7_u8", "_mod8_u8", "_shift12_u8", "_mul12_u8", "_div10_u8"]
rows = {r["function"]: r for r in csv.DictReader(open(sys.argv[1]))}
lines = ["| function | cycles per item |", "| --- | --- |"]
for f in ORDER:
    r = rows[f]
    per = int(r["inclusive"]) / int(r["calls"]) / 64
    lines.append(f"| `{f[1:]}` | {per:.1f} |")
print("\n".join(lines))
if "--md" in sys.argv:
    open(sys.argv[sys.argv.index("--md") + 1], "w").write("\n".join(lines) + "\n")
