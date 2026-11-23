#!/usr/bin/env python3
"""Writes results/summary.md from ai-test/*/*/final.json and round*-result.json."""
import json
import statistics
from collections import defaultdict
from pathlib import Path

HERE = Path(__file__).resolve().parent.parent
NES_PROLOGUE = 13  # PHA TXA PHA TYA PHA before the JSR in the harness NMI handler
# Master System: from line 192 to the CALL (interrupt, handler saves, IN A,($BF)). Calibrated on the
# hand-counted reference palette routine (737 cycles from CALL to its last OUT, last write measured at 1135).
SMS_OFFSET = 398


def load():
    rows = []
    for f in sorted((HERE / "ai-test").glob("*/*/final.json")):
        r = json.loads(f.read_text())
        r["dir"] = f.parent
        r.setdefault("variant", "warned")
        rows.append(r)
    return rows


def why(res):
    if not res["assembled"]:
        return "assembler error"
    out = []
    if not res["timing_ok"]:
        out.append("late")
    if not res["memory_ok"]:
        cs = res["cases"]
        if any(c.get("wrong_bytes") for c in cs):
            out.append("wrong bytes")
        elif any(c.get("dump_diff") for c in cs):
            out.append("stray VRAM writes")
        else:
            out.append("picture differs")
    return ", ".join(out)


def main():
    rows = load()
    lines = ["# Results", ""]
    lines += ["## First try and after fixes", "",
              "| Prompt | Model | CPU | Task | First try | After 1 fix | After 2 fixes | First-try failures |",
              "| --- | --- | --- | --- | --- | --- | --- | --- |"]
    g = defaultdict(list)
    for r in rows:
        g[(r["variant"], r["model"], r["cpu"], r["task"])].append(r)
    tot = defaultdict(lambda: [0, 0, 0, 0])
    for (v, m, cpu, task), rs in sorted(g.items()):
        f0 = sum(r["first_try"] for r in rs)
        f1 = sum(r["ok"] and r["fix_rounds"] <= 1 for r in rs)
        f2 = sum(r["ok"] for r in rs)
        fails = []
        for r in rs:
            if not r["first_try"]:
                res = json.loads((r["dir"] / "round0-result.json").read_text())
                fails.append(f"r{r['rep']}: {why(res)}")
        lines.append(f"| {v} | {m} | {cpu} | {task} | {f0}/{len(rs)} | {f1}/{len(rs)} | {f2}/{len(rs)} | {'; '.join(fails) or '-'} |")
        t = tot[(v if task != "huge" else "huge", m, cpu)]
        t[0] += f0; t[1] += f1; t[2] += f2; t[3] += len(rs)
    lines += ["", "| Prompt | Model | CPU | First try | After 1 fix | After 2 fixes |", "| --- | --- | --- | --- | --- | --- |"]
    for (v, m, cpu), t in sorted(tot.items()):
        lines.append(f"| {v} | {m} | {cpu} | {t[0]}/{t[3]} | {t[1]}/{t[3]} | {t[2]}/{t[3]} |")

    lines += ["", "## Timing of every first try that assembled", "",
              "NES: CPU cycles from the first instruction of the NMI handler to the end of the last PPU write "
              "(OAM DMA counted as 513 cycles); vertical blank ends 2260-2262 cycles after it. Master System: Z80 "
              "cycles from the start of line 192 to the last VDP write; the handler starts at 252 and vertical blank "
              "ends at 15960.", "",
              "| Prompt | Model | CPU | Task | Last write (min-max over reps) | VBlank end | Budget claimed | Cycles claimed | Measured routine cycles |",
              "| --- | --- | --- | --- | --- | --- | --- | --- | --- |"]
    for (v, m, cpu, task), rs in sorted(g.items()):
        ends, claims, used, meas = [], [], [], []
        vbl = None
        for r in rs:
            res = json.loads((r["dir"] / "round0-result.json").read_text())
            rd = r["rounds"][0]
            if rd["budget"] is not None:
                claims.append(rd["budget"])
            if rd["used"] is not None:
                used.append(rd["used"])
            if res.get("work_end_max") is not None:
                ends.append(res["work_end_max"]); vbl = res["vbl_end"]
                meas.append(res["work_end_max"] - (NES_PROLOGUE if cpu == "6502" else SMS_OFFSET))
        rng = lambda v: "-" if not v else (f"{min(v)}" if min(v) == max(v) else f"{min(v)}-{max(v)}")
        lines.append(f"| {v} | {m} | {cpu} | {task} | {rng(ends)} | {vbl} | {rng(claims)} | {rng(used)} | {rng(meas)} |")

    # claimed vs measured, per first try that gave a claim
    diffs = []
    for r in rows:
        rd = r["rounds"][0]
        res = json.loads((r["dir"] / "round0-result.json").read_text())
        if rd["used"] and res.get("work_end_max") is not None:
            m = res["work_end_max"] - (NES_PROLOGUE if r["cpu"] == "6502" else SMS_OFFSET)
            diffs.append((r["cpu"], abs(rd["used"] - m) / m))
    for cpu in ("6502", "z80"):
        d = [x for c, x in diffs if c == cpu]
        if d:
            lines.append("")
            lines.append(f"{cpu}: cycle claims within 1% of the measurement in {sum(x <= 0.01 for x in d)}/{len(d)} first tries, "
                         f"within 5% in {sum(x <= 0.05 for x in d)}/{len(d)}.")
    cost = sum((rd["cost"] or 0) for r in rows for rd in r["rounds"])
    calls = sum(len(r["rounds"]) for r in rows)
    wall = [rd["wall_s"] for r in rows for rd in r["rounds"]]
    lines += ["", f"Claude calls: {calls}, cost at API prices USD {cost:.2f}, median wall time per call "
              f"{statistics.median(wall):.0f} s." if wall else ""]
    (HERE / "results" / "summary.md").write_text("\n".join(lines) + "\n")
    print("\n".join(lines))


if __name__ == "__main__":
    main()
