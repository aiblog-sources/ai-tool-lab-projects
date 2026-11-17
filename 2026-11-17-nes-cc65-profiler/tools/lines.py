#!/usr/bin/env python3
"""lines.py rom.dbg pcs.csv frames [--src DIR] [--top N] [--md out.md] [--own]
lines.py rom.dbg spikes.csv 0 --spikes [--src DIR] [--top N] [--md out.md]

Per C source line cycles from nesprof --pcs and the ld65 debug file: each
instruction address goes to the C line cc65 recorded for it (line records
with type=1), falling back to the assembly line. Prints the hottest lines
with cycles per frame, share of the busy cycles, and the source text when
the file is found under --src. By default a line's cycles include the cc65
runtime helpers it called (pushax, shlax2, tosmulax...); --own leaves them
as rows of their own. With --spikes the input is nesprof --spikes output and
the table lists, for each of the slowest game loops, its busy cycles and its
--top hottest lines (cycles in that loop, not per frame). Assembly (the cc65 runtime, crt0) gets one
row per routine. Waiting (_wait_frame) is left out. MIT.
"""
import argparse
import csv
import re
from pathlib import Path


def fields(s):
    return {m.group(1): (m.group(3) if m.group(3) is not None else m.group(2))
            for m in re.finditer(r'(\w+)=("([^"]*)"|[^,]*)', s)}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("dbg"); ap.add_argument("pcs"); ap.add_argument("frames", type=int)
    ap.add_argument("--src", default="."); ap.add_argument("--top", type=int, default=15)
    ap.add_argument("--md"); ap.add_argument("--idle", default="_wait_frame")
    ap.add_argument("--own", action="store_true", help="each address's own cycles, helpers as their own rows")
    ap.add_argument("--spikes", action="store_true", help="input is nesprof --spikes output")
    a = ap.parse_args()
    files, segs, spans, lines = {}, {}, {}, []
    for raw in open(a.dbg):
        kind, _, rest = raw.rstrip("\n").partition("\t")
        f = fields(rest)
        if kind == "file": files[f["id"]] = f["name"]
        elif kind == "seg": segs[f["id"]] = int(f["start"], 16)
        elif kind == "span": spans[f["id"]] = (segs[f["seg"]] + int(f["start"]), int(f["size"]))
        elif kind == "line" and "span" in f: lines.append(f)
    where = {}
    for ln in sorted(lines, key=lambda l: l.get("type") == "1"):   # C lines last, so they win
        for sid in ln["span"].split("+"):
            start, size = spans[sid]
            for addr in range(start, start + size):
                where[addr] = (files[ln["file"]], int(ln["line"]))
    def key_of(addr, function):
        key = where.get(addr, ("?", 0))
        return key if key[0].endswith(".c") else (function, 0)

    src_cache = {}

    def text(fname, n):
        if fname not in src_cache:
            p = Path(a.src) / Path(fname).name
            src_cache[fname] = p.read_text().splitlines() if p.exists() else []
        s = src_cache[fname]
        return s[n - 1].strip() if 0 < n <= len(s) else ""

    def loc(key):
        return f"{Path(key[0]).name}:{key[1]}" if key[1] else "(asm)"

    if a.spikes:
        loops = {}
        for r in csv.DictReader(open(a.pcs)):
            if r["function"] == a.idle:
                continue
            lp = loops.setdefault(int(r["loop"]), {"frame": int(r["end_frame"]), "busy": int(r["busy"]),
                                                    "nmis": int(r["nmis"]), "lines": {}, "fn": {}})
            k = key_of(int(r["addr"], 16), r["function"])
            lp["lines"][k] = lp["lines"].get(k, 0) + int(r["cycles"])
            lp["fn"].setdefault(k, r["function"])
        out = ["| loop (ends at frame) | busy cycles | NMIs | hottest lines in that loop |", "| --- | --- | --- | --- |"]
        for n, lp in sorted(loops.items(), key=lambda kv: -kv[1]["busy"]):
            hot = sorted(lp["lines"].items(), key=lambda kv: -kv[1])[:a.top]
            cells = "<br>".join(f"{loc(k)} `{text(*k)}` {c:,}".replace("|", "\\|") if k[1] else f"`{lp['fn'][k]}` {c:,}"
                                for k, c in hot)
            out.append(f"| {n} ({lp['frame']}) | {lp['busy']:,} | {lp['nmis']} | {cells} |")
        print("\n".join(out))
        if a.md:
            Path(a.md).write_text("\n".join(out) + "\n")
        return
    cyc, fn_of, busy = {}, {}, 0
    col = "cycles" if a.own else "with_helpers"
    for r in csv.DictReader(open(a.pcs)):
        if r["function"] == a.idle or not int(r[col]):
            continue
        addr = int(r["addr"], 16)
        key = key_of(addr, r["function"])      # runtime and startup code: one row per routine
        cyc[key] = cyc.get(key, 0) + int(r[col])
        fn_of.setdefault(key, r["function"])
        busy += int(r[col])
    out = ["| line | function | cycles/frame | busy | source |", "| --- | --- | --- | --- | --- |"]
    for key, c in sorted(cyc.items(), key=lambda kv: -kv[1])[:a.top]:
        src = text(*key).replace("|", "\\|")
        out.append(f"| {loc(key)} | `{fn_of[key]}` | {c / a.frames:,.0f} | {c / busy:.1%} | `{src}` |")
    print("\n".join(out))
    if a.md:
        Path(a.md).write_text("\n".join(out) + "\n")


if __name__ == "__main__":
    main()
