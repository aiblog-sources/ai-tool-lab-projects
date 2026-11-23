#!/usr/bin/env python3
"""Writes results/huge.md: what each model did with the copy that cannot fit, round by round.

Strategy is read from the checker's result: late = it ran past vertical blank; truncated = on time but the
tail of the destination was never written; two frames = on time and the whole destination right after
consecutive frames (the routine alternates halves or windows). Whether each first reply said the job
cannot fit was read by hand (all 12 did; see the round0-reply.md files)."""
import json
from pathlib import Path

HERE = Path(__file__).resolve().parent.parent


def strategy(res):
    if not res["assembled"]:
        return "did not assemble"
    if not res["timing_ok"]:
        return "late"
    return "two frames" if res["memory_ok"] else "truncated"


def main():
    out = ["# The copy that cannot fit (task `huge`)", "",
           "NES: OAM DMA + 256 bytes; Master System: 1024 bytes. Last write in cycles (NES: from the NMI handler's "
           "first instruction, vertical blank ends at 2260-2263; Master System: from line 192, ends at 15960).", "",
           "| Model | CPU | Rep | Round 0 | Round 1 | Round 2 |", "| --- | --- | --- | --- | --- | --- |"]
    for m in ("claude-sonnet-5-5", "claude-opus-5-5"):
        for cpu in ("6502", "z80"):
            for rep in (1, 2, 3):
                d = HERE / "ai-test" / m / f"{cpu}-huge-r{rep}"
                cells = []
                for rnd in range(3):
                    f = d / f"round{rnd}-result.json"
                    if not f.exists():
                        cells.append("")
                        continue
                    res = json.loads(f.read_text())
                    wrong = sum(c.get("wrong_bytes", 0) for c in res["cases"]) // len(res["cases"])
                    cells.append(f"{strategy(res)}, last write {res.get('work_end_max')}" +
                                 (f", {wrong} bytes missing or wrong" if wrong else ""))
                out.append(f"| {m} | {cpu} | {rep} | " + " | ".join(cells) + " |")
    (HERE / "results" / "huge.md").write_text("\n".join(out) + "\n")
    print("\n".join(out))


if __name__ == "__main__":
    main()
