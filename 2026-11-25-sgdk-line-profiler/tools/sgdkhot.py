#!/usr/bin/env python3
"""sgdkhot: which lines of an SGDK game really cost the frame, and which lint findings matter.

1. builds the project with the SGDK Docker image and EXTRA_FLAGS=-g (debug info only; the ROM is byte-identical)
2. runs it headless in mdlines (Genesis Plus GX + a shadow call stack) with a scripted pad
3. maps every call site to a line of the game's own C code (addr2line -i: library code that LTO inlined into a
   game function is charged to the game line that used it)
4. runs sgdklint on the same source and joins its findings to the measured lines

usage: sgdkhot.py PROJECT [--frames 1260] [--from 60] [--input F-T:BTN ...] [--no-build] [--json out.json]
                          [--lint path/to/sgdklint.py] [--top 15]
Needs Docker, the SGDK image, mdlines (profiler/build.sh) and sgdklint (2026-10-29 project).
"""
import argparse
import csv
import json
import os
import re
import subprocess
import sys
from collections import defaultdict
from pathlib import Path

HERE = Path(__file__).resolve().parent.parent
WORK = Path(os.environ.get("WORK", Path.home() / ".cache/mdprof"))
IMG = os.environ.get("SGDK_IMAGE", "ghcr.io/stephane-d/sgdk:latest")
LINT = HERE.parent / "2026-10-29-sgdk-perf-lint" / "lint" / "sgdklint.py"
UID = f"{os.getuid()}:{os.getgid()}"
FRAME = 128005  # 68000 cycles per NTSC frame (H40)
# SGDK's own once-per-frame work: listed with the lines, never as an unflagged hot spot
FRAME_WORK = ("SYS_doVBlankProcess",)


def build(proj):
    p = subprocess.run(["docker", "run", "--rm", "--network", "none", "--user", UID, "-e", "EXTRA_FLAGS=-g",
                        "-v", f"{proj}:/src", IMG], capture_output=True, text=True, timeout=600)
    if not (proj / "out" / "rom.out").exists():
        sys.exit("build failed:\n" + (p.stdout + p.stderr)[-3000:])


def profile(proj, frames, start, inputs, extra=()):
    args = ["docker", "run", "--rm", "--network", "none", "--user", UID, "-v", f"{WORK}:/w", "-v", f"{proj}:/p",
            "-w", "/w", "gcc:14", "./mdlines", "gpgx/genesis_plus_gx_libretro.so", "/p/out/rom.bin",
            "/p/out/symbol.txt", str(frames), "--from", str(start), "--lines", "/p/out/lines.csv",
            "--json", "/p/out/prof.json", "--csv", "/p/out/frames.csv", *extra]
    for i in inputs:
        args += ["--input", i]
    p = subprocess.run(args, capture_output=True, text=True, timeout=900)
    if p.returncode:
        sys.exit("mdlines failed:\n" + p.stdout + p.stderr)
    return p.stdout


def read_lines(path):
    meta = {}
    rows = []
    with open(path) as f:
        first = f.readline()
        for kv in first[1:].split():
            k, v = kv.split("=")
            meta[k] = int(v)
        for r in csv.DictReader(f):
            rows.append(dict(site=int(r["site"], 16), callee=r["callee"], cycles=int(r["cycles"]),
                             maxf=int(r["max_frame_cycles"]), calls=int(r["calls"])))
    return meta, rows


def addr2line(proj, addrs):
    """address -> (function, 'main.c', line) of the outermost game-code frame in the inline chain"""
    out = {}
    addrs = sorted(addrs)
    for i in range(0, len(addrs), 400):
        chunk = addrs[i:i + 400]
        p = subprocess.run(["docker", "run", "--rm", "--network", "none", "--entrypoint", "m68k-elf-addr2line",
                            "-v", f"{proj}:/p", IMG, "-a", "-f", "-i", "-e", "/p/out/rom.out"]
                           + [hex(a) for a in chunk], capture_output=True, text=True, timeout=300)
        cur = None
        lines = p.stdout.splitlines()
        k = 0
        while k < len(lines):
            l = lines[k]
            if l.startswith("0x"):
                cur = int(l, 16)
                out[cur] = None
                k += 1
                continue
            fn, loc = l, lines[k + 1] if k + 1 < len(lines) else "??:0"
            k += 2
            m = re.match(r"(.*):(\d+)", loc.split(" ")[0])
            if not m:
                continue
            path, line = m.group(1), int(m.group(2))
            # the chain goes from innermost to outermost; the first game-code entry wins: SGDK code that LTO
            # inlined into the game is skipped, and a game helper inlined into main keeps its own line
            if out[cur] is None and path.startswith("/src/src/") and not path.startswith("/src/src/boot/"):
                out[cur] = (fn, path[len("/src/"):], line)
    return out


def run_lint(proj, lint_path):
    tmp = proj / "out" / "lint.json"
    p = subprocess.run([sys.executable, str(lint_path), "--json", str(tmp), "src"], capture_output=True,
                       text=True, cwd=proj)
    return json.loads(tmp.read_text())["findings"], p.stdout


def analyse(proj, meta, rows, lint_findings):
    frames = meta["counted_frames"]
    busy = sum(r["cycles"] for r in rows)
    sites = {r["site"] for r in rows if r["site"]}
    loc = addr2line(proj, sites)
    lines = defaultdict(lambda: dict(cycles=0, self=0, calls=defaultdict(lambda: [0, 0]), fn=None))
    unmapped = 0
    for r in rows:
        where = loc.get(r["site"]) if r["site"] else None
        if not where:
            key = ("[no game line]", 0)
            unmapped += r["cycles"]
        else:
            key = (where[1], where[2])
        e = lines[key]
        e["cycles"] += r["cycles"]
        if where:
            e["fn"] = where[0]
        if r["callee"] and where:
            c = e["calls"][r["callee"]]
            c[0] += r["cycles"]
            c[1] += r["calls"]
        elif where:
            e["self"] += r["cycles"]
        else:  # library code that ran with no game line on the stack: show it by function
            e["calls"][r["callee"] or "?"][0] += r["cycles"]
    by_line = {}
    for (f, l), e in lines.items():
        by_line[(f, l)] = dict(file=f, line=l, function=e["fn"], per_frame=e["cycles"] / frames,
                               self_per_frame=e["self"] / frames, pct_busy=100 * e["cycles"] / busy if busy else 0,
                               calls=sorted(([k, v[0] / frames, v[1] / frames, v[0] / v[1] if v[1] else None]
                                             for k, v in e["calls"].items()), key=lambda x: -x[1]))
    joined = []
    for fd in lint_findings:
        m = by_line.get((fd["file"], fd["line"]))
        pf = m["per_frame"] if m else 0.0
        verdict = ("not run" if not m else "matters" if pf >= 0.01 * FRAME else
                   "small" if pf >= 0.002 * FRAME else "negligible")
        joined.append(dict(fd, measured_per_frame=round(pf, 1),
                           measured_pct_frame=round(100 * pf / FRAME, 2), verdict=verdict,
                           calls=m["calls"][:3] if m else []))
    flagged = {(fd["file"], fd["line"]) for fd in lint_findings}
    ranked = sorted(by_line.values(), key=lambda x: -x["per_frame"])
    return dict(frames=frames, busy_per_frame=busy / frames, unmapped_per_frame=unmapped / frames,
                stack_overflow=meta.get("stack_overflow", 0), lines=ranked, findings=joined,
                unflagged_hot=[x for x in ranked if x["file"].startswith("src/")
                               and (x["file"], x["line"]) not in flagged and x["per_frame"] >= 0.02 * FRAME
                               and not (x["calls"] and x["calls"][0][0].startswith(FRAME_WORK))])


def fmt(n):
    return f"{n:,.0f}"


def report(a, src_dir, top):
    src = {}
    out = []
    w = out.append
    w("== Lines by measured cost (sgdkhot: mdlines + sgdklint) ==")
    w(f"budget {FRAME:,} cycles per video frame (NTSC); {a['frames']:,} frames counted; busy "
      f"{fmt(a['busy_per_frame'])} cycles per frame on average ({100 * a['busy_per_frame'] / FRAME:.1f}%)")
    w("each line = its own instructions + every library/libgcc call made from it (game helpers count on their own lines);")
    w("DMA from 68000 memory stalls the CPU, so a line that starts a DMA also carries the stall")
    fl = defaultdict(list)
    for fd in a["findings"]:
        fl[(fd["file"], fd["line"])].append(fd)
    for x in a["lines"][:top]:
        if x["per_frame"] < 1:
            break
        f = x["file"]
        if f.startswith("src/") and f not in src:
            p = src_dir.parent / f
            src[f] = p.read_text(errors="replace").splitlines() if p.exists() else []
        text = src.get(f, [])
        code = text[x["line"] - 1].strip()[:60] if 0 < x["line"] <= len(text) else ""
        w(f"{fmt(x['per_frame']):>9} /frame {x['pct_busy']:5.1f}% of busy  {f}:{x['line']}  {code}")
        for name, cyc, calls, each in x["calls"][:3]:
            w(f"{'':>17}{fmt(cyc):>9} in {name}" + (f": {calls:.2f} calls/frame, {fmt(each)} cycles each" if each else ""))
        for fd in fl.get((f, x["line"]), []):
            w(f"{'':>17}lint [{fd['rule']}] {fd['level']}: {fd['message'][:110]}")
    w("")
    w("== Lint findings, by measured cost ==")
    for fd in sorted(a["findings"], key=lambda d: -d["measured_per_frame"]):
        w(f"{fd['verdict']:>10}  {fmt(fd['measured_per_frame']):>8} /frame ({fd['measured_pct_frame']:.2f}% of a frame)  "
          f"{fd['file']}:{fd['line']} [{fd['rule']}] {fd['level']}")
    if not a["findings"]:
        w("  (no findings)")
    w("verdicts: matters >= 1% of a frame, small >= 0.2%, negligible below, not run = no cycles on that line in this run")
    w("")
    w("== Game lines over 2% of a frame with no lint finding ==")
    for x in a["unflagged_hot"]:
        top_call = x["calls"][0][0] if x["calls"] else "own code"
        w(f"{fmt(x['per_frame']):>9} /frame  {x['file']}:{x['line']} ({x['function']}), mostly {top_call}")
    if not a["unflagged_hot"]:
        w("  (none)")
    if a["unmapped_per_frame"]:
        w(f"\nlibrary code with no game line on the stack (interrupt handlers, SGDK's VBlank work): "
          f"{fmt(a['unmapped_per_frame'])} cycles per frame")
    return "\n".join(out)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("project")
    ap.add_argument("--frames", type=int, default=1260)
    ap.add_argument("--from", dest="start", type=int, default=60)
    ap.add_argument("--input", action="append", default=[])
    ap.add_argument("--no-build", action="store_true")
    ap.add_argument("--json")
    ap.add_argument("--lint", default=str(LINT))
    ap.add_argument("--top", type=int, default=15)
    a = ap.parse_args()
    proj = Path(a.project).resolve()
    if not a.no_build:
        build(proj)
    prof_text = profile(proj, a.frames, a.start, a.input)
    meta, rows = read_lines(proj / "out" / "lines.csv")
    findings, _ = run_lint(proj, a.lint)
    res = analyse(proj, meta, rows, findings)
    res["mdprof"] = json.loads((proj / "out" / "prof.json").read_text())
    print(report(res, proj / "src", a.top))
    if a.json:
        Path(a.json).write_text(json.dumps(res, indent=1, default=str))


if __name__ == "__main__":
    main()
