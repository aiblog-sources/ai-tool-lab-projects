#!/usr/bin/env python3
"""Tables for the write-up from results/corpus/ (writes results/summary.md)."""
import json
import statistics as st
from collections import Counter, defaultdict
from pathlib import Path

HERE = Path(__file__).resolve().parent.parent
C = HERE / "results" / "corpus"
OLD = HERE.parent / "2026-10-29-sgdk-perf-lint" / "ai-test"
FRAME = 128005
VERD = ["matters", "small", "negligible", "not run"]


def load(tag, stage):
    p = C / tag / f"{stage}.json"
    return json.loads(p.read_text()) if p.exists() else None


def main():
    tags = sorted(p.name for p in C.iterdir() if (p / "v0.json").exists())
    out = []
    w = out.append
    w(f"# sgdkhot on the 40 games of the 2026-10-29 test\n\nPrograms: {len(tags)}. Verdicts: matters = at least 1% of a "
      "frame (1,280 cycles) per frame on average, small = 0.2 to 1%, negligible = under 0.2%, not run = no cycles on "
      "that line in the 1,200 counted frames.\n")

    # 1. lint findings on v0 by rule and verdict
    by = defaultdict(Counter)
    lvl = defaultdict(Counter)
    for t in tags:
        for fd in load(t, "v0")["findings"]:
            by[fd["rule"]][fd["verdict"]] += 1
            by["all"][fd["verdict"]] += 1
            lvl[fd["level"]][fd["verdict"]] += 1
    w("## Lint findings on the original code (v0), by rule\n")
    w("| Rule | Findings | " + " | ".join(VERD) + " |")
    w("| --- | --- | " + " | ".join("---" for _ in VERD) + " |")
    for r in sorted(by, key=lambda r: (r == "all", -sum(by[r].values()))):
        w(f"| {r} | {sum(by[r].values())} | " + " | ".join(str(by[r][v]) for v in VERD) + " |")
    w("\n## Same, by lint level\n")
    w("| Level | Findings | " + " | ".join(VERD) + " |")
    w("| --- | --- | " + " | ".join("---" for _ in VERD) + " |")
    for l in ("heavy", "hot", "frame", "guarded"):
        if lvl[l]:
            w(f"| {l} | {sum(lvl[l].values())} | " + " | ".join(str(lvl[l][v]) for v in VERD) + " |")

    # 2. lint estimates vs measurement: the cost of one operation, and the cost per frame it implies
    w("\n## Lint estimates vs measurement (div and mul32 findings whose line called libgcc)\n")
    w("Lint per op = its estimate divided by the loop passes it assumed; lint per frame = its estimate (passes x op). "
      "Measured per frame = everything on that line.\n")
    w("| Program | Line | Level | Lint per op | Measured per call | Callee | Calls per frame | Lint per frame | Measured per frame |")
    w("| --- | --- | --- | --- | --- | --- | --- | --- | --- |")
    op_r, fr_r = [], []
    for t in tags:
        for fd in load(t, "v0")["findings"]:
            if fd["rule"] not in ("div", "mul32") or not fd["calls"] or not fd.get("est_cycles"):
                continue
            lib = [c for c in fd["calls"] if c[0].startswith("__")]
            if not lib or not lib[0][3]:
                continue
            name, cyc, calls, each = lib[0]
            per_op = fd["est_cycles"] / max(1, fd.get("trips") or 1)
            op_r.append(each / per_op)
            fr_r.append(fd["measured_per_frame"] / fd["est_cycles"])
            w(f"| {t} | {fd['line']} | {fd['level']} | {per_op:,.0f} | {each:,.0f} | {name} | {calls:.2f} | "
              f"{fd['est_cycles']:,} | {fd['measured_per_frame']:,.0f} |")
    if op_r:
        w(f"\nper call, measured / lint: median {st.median(op_r):.2f}, range {min(op_r):.2f} to {max(op_r):.2f}; "
          f"per frame, measured / lint: median {st.median(fr_r):.3f}, range {min(fr_r):.4f} to {max(fr_r):.2f} "
          f"({len(op_r)} findings)")

    # 3. per program: hottest game line (not SYS_doVBlankProcess) and the share of busy on flagged lines
    w("\n## Per program: busy cycles, share on lint-flagged lines, hottest game line\n")
    w("| Program | Load | Findings | Busy on flagged lines | Hottest game line (cycles/frame) | Flagged? | What it is |")
    w("| --- | --- | --- | --- | --- | --- | --- |")
    shares, hot_flagged = [], 0
    kinds = Counter()
    for t in tags:
        a = load(t, "v0")
        flagged = {fd["line"] for fd in a["findings"]}
        game = [x for x in a["lines"] if x["file"].startswith("src/")
                and not (x["calls"] and x["calls"][0][0].startswith("SYS_doVBlankProcess"))]
        busy = a["busy_per_frame"]
        share = 100 * sum(x["per_frame"] for x in a["lines"] if x["file"].startswith("src/")
                          and x["line"] in flagged) / busy
        shares.append(share)
        h = game[0]
        is_f = h["line"] in flagged
        hot_flagged += is_f
        what = h["calls"][0][0] if h["calls"] and h["calls"][0][1] > h["self_per_frame"] else "own code (incl. inlined SGDK)"
        kinds[what if what.startswith("own") else what.split(".")[0]] += 1
        w(f"| {t} | {a['mean_load']:.1f}% | {len(a['findings'])} | {share:.1f}% | main.c:{h['line']} ({h['per_frame']:,.0f}) "
          f"| {'yes' if is_f else 'no'} | {what} |")
    w(f"\nbusy cycles on flagged lines: median {st.median(shares):.1f}%, range {min(shares):.1f} to {max(shares):.1f}%; "
      f"hottest game line flagged in {hot_flagged} of {len(tags)}")
    w("what the hottest game line is: " + ", ".join(f"{k} {v}" for k, v in kinds.most_common()))
    sysv = [x["per_frame"] for t in tags for x in load(t, "v0")["lines"]
            if x["calls"] and x["calls"][0][0].startswith("SYS_doVBlankProcess")]
    w(f"SYS_doVBlankProcess line: {min(sysv):,.0f} to {max(sysv):,.0f} cycles per frame ({len(sysv)} lines)")

    # 4. fix rounds
    w("\n## Fix rounds from the same v0 code (load in % of a frame, mean over programs)\n")
    w("`lint2` and `plain` are the 2026-10-29 rounds (re-profiled here, same numbers); `hot` is the new round: the "
      "`plain` prompt plus the sgdkhot report of v0.\n")
    w("| Model | Stage | Programs built | Mean load v0 | Mean load after | Mean change | Programs faster / slower | Dropped frames after (sum) | USD |")
    w("| --- | --- | --- | --- | --- | --- | --- | --- | --- |")
    models = sorted({json.loads((OLD / t / "result.json").read_text())["model"] for t in tags})
    for m in models:
        mt = [t for t in tags if json.loads((OLD / t / "result.json").read_text())["model"] == m]
        for stage in ("lint2", "plain", "hot"):
            pairs, drop, usd = [], 0, 0.0
            for t in mt:
                v0, s = load(t, "v0"), load(t, stage)
                if stage == "hot" and (C / t / "hot-result.json").exists():
                    usd += json.loads((C / t / "hot-result.json").read_text()).get("cost_usd", 0)
                if not s or not s.get("builds"):
                    continue
                pairs.append((v0["mean_load"], s["mean_load"]))
                drop += s["dropped"]
            if not pairs:
                continue
            ch = [b - a for a, b in pairs]
            w(f"| {m.replace('claude-', '')} | {stage} | {len(pairs)} of {len(mt)} | {st.mean(a for a, _ in pairs):.2f} | "
              f"{st.mean(b for _, b in pairs):.2f} | {st.mean(ch):+.2f} | {sum(c < -0.05 for c in ch)} / "
              f"{sum(c > 0.05 for c in ch)} | {drop} | " + (f"{usd:.2f} |" if stage == "hot" else "see 2026-10-29 |"))
    # 5. hot vs plain, program by program
    b = wo = same = 0
    for t in tags:
        p, h = load(t, "plain"), load(t, "hot")
        if not p or not h or not h.get("builds"):
            continue
        if h["mean_load"] < p["mean_load"] - 0.05:
            b += 1
        elif h["mean_load"] > p["mean_load"] + 0.05:
            wo += 1
        else:
            same += 1
    w(f"\nhot vs plain, program by program (0.05-point margin): hot lower in {b}, higher in {wo}, same in {same}")
    (HERE / "results" / "summary.md").write_text("\n".join(out) + "\n")
    print("\n".join(out))


if __name__ == "__main__":
    main()
