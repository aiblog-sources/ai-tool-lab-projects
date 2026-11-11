"""mdperf: run an SGDK ROM through mddma (DMA budget) and mdprof (CPU per function) with the same scripted pad,
and write one report and one per-frame CSV with both.

2026-11-11: mdprof is now the `mdprof-loops` build (../profiler, adds --fncsv), and the report gains a "game loops
by function" section: the functions of each game loop added up over every video frame the loop spans, so a loop
that takes two frames is no longer read as one frame capped at 100%.

Both checkers are libretro frontends for Genesis Plus GX, built by the 2026-11-07 (mddma) and 2026-10-28 (mdprof)
projects in this repo. Emulation is deterministic, so two runs with the same ROM and inputs see the same frames;
mdperf checks that by comparing the per-frame CPU load both tools logged.

usage: mdperf.py <sgdk project dir with out/rom.bin and out/symbol.txt> [--frames 1500] [--from 700]
                 [--input F-T:BTN ...] [--shot N] [--out DIR]
env:   MDDMA_WORK (default ~/.cache/mddma), MDPROF_WORK (default ~/.cache/mdprof)
"""
import argparse
import csv
import os
import shutil
import subprocess
import sys
from pathlib import Path

MDDMA = Path(os.environ.get("MDDMA_WORK", Path.home() / ".cache/mddma"))
MDPROF = Path(os.environ.get("MDPROF_WORK", Path.home() / ".cache/mdprof"))
UID = f"{os.getuid()}:{os.getgid()}"
HERE = Path(__file__).resolve().parent


def run_tool(work, exe, proj, tag, frames, start, inputs, shot=None, extra=()):
    """Copy the ROM into the tool's work dir (the container sees only that dir) and run the tool there."""
    dst = work / "perf" / tag
    if dst.exists():
        shutil.rmtree(dst)
    (dst / "out").mkdir(parents=True)
    for f in ("rom.bin", "symbol.txt"):
        shutil.copy(proj / "out" / f, dst / "out" / f)
    rel = f"perf/{tag}"
    args = ["docker", "run", "--rm", "--network", "none", "--user", UID, "-v", f"{work}:/w", "-w", "/w", "gcc:14",
            f"./{exe}", "gpgx/genesis_plus_gx_libretro.so", f"{rel}/out/rom.bin", f"{rel}/out/symbol.txt",
            str(frames), "--from", str(start), "--csv", f"{rel}/frames.csv"]
    if shot is not None:
        args += ["--shot", f"{shot}:{rel}/shot.ppm"]
    for i in inputs:
        args += ["--input", i]
    args += [a.replace("{rel}", rel) for a in extra]
    p = subprocess.run(args, capture_output=True, text=True, timeout=600)
    text = "\n".join(l for l in (p.stdout + p.stderr).splitlines() if not l.startswith("note: idle function"))
    if p.returncode or not (dst / "frames.csv").exists():
        raise RuntimeError(f"{exe} failed (exit {p.returncode}): {text[-400:]}")
    return text, dst


def trim_prof(text, busiest=3):
    """mdprof's report with only the first few busiest frames, and without the wall-clock line."""
    out, n = [], 0
    for l in text.splitlines():
        if l.startswith("frames ") and "wall" in l:
            l = l.split(", wall")[0]
        if l.startswith("  frame "):
            n += 1
        if n > busiest:
            continue
        out.append(l)
    return "\n".join(out).rstrip()


def loop_costs(prof, start):
    """Cost of each game loop: busy cycles from the frame it started in to the frame before the next one started.
    mdprof reports per video frame, capped at 100%; a loop longer than a frame needs this view."""
    fr = [f for f in sorted(prof) if f >= start]
    starts = [f for f in fr if prof[f]["loop_start"] == "1"]
    budget = int(prof[fr[0]]["frame_cycles"])
    loops = []
    for a, b in zip(starts, starts[1:]):
        loops.append(dict(frame=a, frames=b - a, busy=sum(int(prof[f]["busy_cycles"]) for f in range(a, b))))
    return budget, loops


def loop_text(budget, loops, show=3):
    by_len = {}
    for l in loops:
        by_len.setdefault(l["frames"], []).append(l["busy"])
    lines = [f"budget: {budget:,} cycles per video frame (NTSC); a game loop that needs more misses the next "
             f"VBlank and the frame is shown twice", f"game loops measured: {len(loops)}"]
    for n in sorted(by_len):
        b = by_len[n]
        lines.append(f"  {len(b)} loops took {n} video frame{'s' if n > 1 else ''}: busy cycles {min(b):,} to "
                     f"{max(b):,} (mean {round(sum(b) / len(b)):,}, {100 * max(b) / budget:.0f}% of one frame at most)")
    worst = sorted(loops, key=lambda l: -l["busy"])[:show]
    if worst:
        lines.append("most expensive loops:")
        for l in worst:
            over = l["busy"] - budget
            lines.append(f"  loop starting in frame {l['frame']}: {l['busy']:,} busy cycles over {l['frames']} frames"
                         + (f", {over:,} more than one frame" if over > 0 else ""))
    return "\n".join(lines)


def loop_functions(fn_rows, loops):
    """Each loop's cycles per function, added up over the frames from its start to the frame before the next start."""
    per_frame = {}
    for r in fn_rows:
        per_frame.setdefault(int(r["frame"]), {})[r["function"]] = int(r["cycles"])
    for l in loops:
        fns = {}
        for f in range(l["frame"], l["frame"] + l["frames"]):
            for name, c in per_frame.get(f, {}).items():
                fns[name] = fns.get(name, 0) + c
        l["fns"] = sorted(fns.items(), key=lambda kv: -kv[1])
    return loops


def loopfn_text(budget, loops, show=3, top=6):
    by_len = {}
    for l in loops:
        by_len.setdefault(l["frames"], []).append(l)
    lines = [f"budget: {budget:,} cycles per video frame (NTSC); a game loop that needs more misses the next "
             f"VBlank and the frame is shown twice",
             f"game loops measured: {len(loops)}; cycles below are added up over every video frame a loop spans"]
    for n in sorted(by_len):
        b = [l["busy"] for l in by_len[n]]
        lines.append(f"  {len(b)} loops took {n} video frame{'s' if n > 1 else ''}: busy cycles {min(b):,} to "
                     f"{max(b):,} (mean {round(sum(b) / len(b)):,})")

    def block(title, l):
        out = [f"{title}: loop starting in frame {l['frame']}, {l['busy']:,} busy cycles over {l['frames']} "
               f"video frame{'s' if l['frames'] > 1 else ''} ({100 * l['busy'] / budget:.0f}% of one frame)"]
        for name, c in l["fns"][:top]:
            out.append(f"    {c:>9,}  {name}")
        return out
    worst = sorted(loops, key=lambda l: -l["busy"])[:show]
    for i, l in enumerate(worst):
        lines += block(f"most expensive loop #{i + 1}", l)
    if loops:
        lines += block("cheapest loop, for comparison", min(loops, key=lambda l: l["busy"]))
    return "\n".join(lines)


def analyze(proj, tag, frames=1500, start=700, inputs=(), shot=None, out=None):
    dma_text, ddir = run_tool(MDDMA, "mddma", proj, tag, frames, start, inputs, shot)
    prof_text, pdir = run_tool(MDPROF, "mdprof-loops", proj, tag, frames, start, inputs,
                               extra=["--fncsv", "{rel}/fns.csv"])
    dma = {int(r["frame"]): r for r in csv.DictReader(open(ddir / "frames.csv"))}
    prof = {int(r["frame"]): r for r in csv.DictReader(open(pdir / "frames.csv"))}
    steady = [f for f in sorted(prof) if f >= start]
    loops = sum(int(prof[f]["loop_start"]) for f in steady)
    loads = sorted(float(prof[f]["load_pct"]) for f in steady)
    # same emulation in both tools: the per-frame CPU load must agree
    mismatch = sum(1 for f in steady if f in dma and abs(float(dma[f]["load_pct"]) - float(prof[f]["load_pct"])) > 0.15)
    summary = dict(frames=len(steady), loops=loops, fps=round(60 * loops / len(steady), 1),
                   dropped=len(steady) - loops, mean_load=round(sum(loads) / len(loads), 1),
                   p95_load=loads[int(0.95 * (len(loads) - 1))],
                   spill_frames=sum(1 for f in steady if f in dma and int(dma[f]["sent_active"])),
                   max_queued=max([int(dma[f]["queued"]) for f in steady if f in dma] + [0]),
                   load_mismatch_frames=mismatch)
    prof_short = trim_prof(prof_text)
    budget, loops = loop_costs(prof, start)
    ltext = loop_text(budget, loops)
    summary["max_loop_cycles"] = max([l["busy"] for l in loops] + [0])
    fn_rows = list(csv.DictReader(open(pdir / "fns.csv")))
    loop_functions(fn_rows, loops)
    # the functions of a loop must add up to its busy cycles (each frame is rounded down per function)
    summary["loopfn_max_gap"] = max([abs(l["busy"] - sum(c for _, c in l["fns"])) for l in loops] + [0])
    lftext = loopfn_text(budget, loops)
    combined = (f"mdperf: frames {start} to {frames - 1} ({len(steady)} frames), NTSC 60 Hz\n"
                f"game loops per second: {summary['fps']} ({summary['dropped']} of {len(steady)} frames dropped)\n\n"
                f"== DMA (mddma) ==\n{dma_text.strip()}\n\n== CPU by function (mdprof) ==\n{prof_short}\n")
    combined2 = combined + f"\n== Game loops (mdperf) ==\n{ltext}\n"
    combined3 = combined + f"\n== Game loops by function (mdperf) ==\n{lftext}\n"
    rate = (f"game loops per second: {summary['fps']} ({summary['dropped']} of {len(steady)} frames dropped, "
            f"frames {start} to {frames - 1}, NTSC 60 Hz)\n")
    if out:
        out = Path(out)
        out.mkdir(parents=True, exist_ok=True)
        (out / "report-dma.txt").write_text(dma_text.strip() + "\n")
        (out / "report-prof.txt").write_text(prof_short + "\n")
        (out / "report.txt").write_text(combined)
        (out / "report-loops.txt").write_text(combined2)
        (out / "report-loopfn.txt").write_text(combined3)
        shutil.copy(pdir / "fns.csv", out / "fns.csv")
        keys = ["frame", "loop_start", "load_pct", "queued", "sent_vblank", "sent_active", "last_active_line",
                "sat_line", "dma_vblank_bytes", "dma_active_bytes"]
        with open(out / "frames.csv", "w", newline="") as f:
            wr = csv.writer(f)
            wr.writerow(keys)
            for fr in sorted(prof):
                row = {**dma.get(fr, {}), **prof[fr]}
                wr.writerow([row.get(k, "") for k in keys])
        if shot is not None and (ddir / "shot.ppm").exists():
            subprocess.run([sys.executable, str(HERE / "ppm2png.py"), str(ddir / "shot.ppm"), "--out", str(out)])
    return summary, dict(dma=dma_text.strip(), prof=prof_short, both=combined, loops=combined2, loopfn=combined3,
                         rate=rate)


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("project")
    ap.add_argument("--frames", type=int, default=1500)
    ap.add_argument("--from", dest="start", type=int, default=700)
    ap.add_argument("--input", action="append", default=[])
    ap.add_argument("--shot", type=int)
    ap.add_argument("--out")
    a = ap.parse_args()
    s, texts = analyze(Path(a.project).resolve(), Path(a.project).resolve().name, a.frames, a.start, a.input,
                       a.shot, a.out)
    print(texts["loopfn"])
    print(s)
