#!/usr/bin/env python3
"""Asks Claude for each VBlank routine on each CPU, runs it in the harness,
and gives it up to two fix rounds with the emulator's report.

  1. `claude -p` (tools off, no CLAUDE.md, no settings) gets the context
     (NMI / VBlank interrupt, PPU or VDP state) and the task, and replies with
     one code block plus two lines: the VBlank budget it assumed and the
     cycles it thinks it uses.
  2. tools/vbrun.py builds it into the harness for 3 cases and checks video
     memory, the picture, and the timing of every write.
  3. If anything failed, a new prompt carries the task, the previous code and
     the report. At most two such rounds.

Results land in ai-test/<model>/<cpu>-<task>-r<rep>/ and ai-test/<model>/summary.jsonl.
usage: ai_test.py [--model claude-sonnet-5-5] [--reps 3] [--workers 4] [--only task,task] [--cpus 6502,z80]
"""
import argparse
import json
import os
import re
import subprocess
import sys
import threading
import time
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

HERE = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(HERE / "tools"))
import tasks  # noqa: E402
import vbrun  # noqa: E402

ENV = dict(os.environ, CLAUDE_CODE_DISABLE_CLAUDE_MDS="1", CLAUDE_CODE_DISABLE_AUTO_MEMORY="1")
SYSTEM = {
    "6502": "You write 6502 assembly for the NES, assembled with ca65 from cc65.",
    "z80": "You write Z80 assembly for the Sega Master System, assembled with sjasmplus.",
}
lock = threading.Lock()
MAX_FIX = 2


def log(*a):
    with lock:
        print(time.strftime("%H:%M:%S"), *a, flush=True)


def prompt(cpu, name, variant="warned"):
    spec = tasks.TASKS[cpu][name]
    if variant == "plain":
        return plain_prompt(cpu, spec)
    if cpu == "6502":
        return f"""Write a 6502 routine for the NES (NTSC) in ca65 syntax.

Routine label: `vblank_update`. It is called with JSR and must return with RTS.

Context: {tasks.NES_CONTRACT}

Task: {spec}

Rules:
- You may change A, X and Y, use zero page $10-$1F as scratch, and use the stack. Do not write any other RAM.
- The code runs from ROM, so no self-modifying code. Tables in ROM are fine (after the code, with .byte).
- Your file is included into the harness's CODE segment: no .segment, .org, header or vectors. Prefix your labels with `vblank_update_` or use @cheap local labels.

Reply with the whole routine in one ```asm code block. After the block, write two lines:
`VBlank budget: N` (how many CPU cycles of vertical blank you assumed an NTSC NES has)
`Cycles used: N` (your count from the JSR to the end of the routine's last PPU register write or OAM DMA)"""
    return f"""Write a Z80 routine for the Sega Master System (NTSC) in sjasmplus syntax.

Routine label: `vblank_update`. It is called with CALL and must return with RET.

Context: {tasks.SMS_CONTRACT}

Task: {spec}

Rules:
- You may change any register, use RAM $C010-$C01F as scratch, and use the stack. Do not write any other RAM.
- The code runs from ROM, so no self-modifying code. Tables in ROM are fine (after the code, with DB).
- Your file is included into the harness after its code: no ORG, OUTPUT, DEVICE or header. Prefix your labels with `vblank_update_` or use local labels (.label).

Reply with the whole routine in one ```asm code block. After the block, write two lines:
`VBlank budget: N` (how many Z80 cycles of vertical blank you assumed an NTSC Master System has)
`Cycles used: N` (your count from the CALL to the end of the routine's last VDP port write)"""


def plain_prompt(cpu, spec):
    """The same task and facts without "finish inside vertical blank", without the scroll rule spelled out,
    and without the budget and cycle lines (asking for them would be a hint)."""
    if cpu == "6502":
        return f"""Write a 6502 routine for the NES (NTSC) in ca65 syntax.

Routine label: `vblank_update`. It is called with JSR and must return with RTS.

Context: {tasks.NES_PLAIN}

Task: {spec}

Rules:
- You may change A, X and Y, use zero page $10-$1F as scratch, and use the stack. Do not write any other RAM.
- The code runs from ROM, so no self-modifying code. Tables in ROM are fine (after the code, with .byte).
- Your file is included into the harness's CODE segment: no .segment, .org, header or vectors. Prefix your labels with `vblank_update_` or use @cheap local labels.

Reply with the whole routine in one ```asm code block."""
    return f"""Write a Z80 routine for the Sega Master System (NTSC) in sjasmplus syntax.

Routine label: `vblank_update`. It is called with CALL and must return with RET.

Context: {tasks.SMS_PLAIN}

Task: {spec}

Rules:
- You may change any register, use RAM $C010-$C01F as scratch, and use the stack. Do not write any other RAM.
- The code runs from ROM, so no self-modifying code. Tables in ROM are fine (after the code, with DB).
- Your file is included into the harness after its code: no ORG, OUTPUT, DEVICE or header. Prefix your labels with `vblank_update_` or use local labels (.label).

Reply with the whole routine in one ```asm code block."""


def fix_prompt(cpu, name, code, res, variant):
    tail = "the whole routine in one code block, then the two lines." if variant != "plain" else \
        "the whole routine in one code block."
    return (prompt(cpu, name, variant) + "\n\nYour previous answer was:\n```asm\n" + code + "\n```\n\n" + vbrun.report(res) +
            "\n\nFix the routine. Reply the same way: " + tail)


def ask(model, cpu, text, cwd):
    for attempt in range(4):
        t0 = time.time()
        p = subprocess.run(["claude", "-p", "--model", model, "--system-prompt", SYSTEM[cpu], "--tools", "",
                            "--strict-mcp-config", "--setting-sources", "", "--output-format", "json",
                            "--no-session-persistence"],
                           input=text, capture_output=True, text=True, env=ENV, cwd=cwd, timeout=1200)
        try:
            d = json.loads(p.stdout)
            if model not in d.get("modelUsage", {}) or d.get("is_error"):
                raise ValueError(f"model {list(d.get('modelUsage', {}))} error {d.get('is_error')}")
            d["wall_s"] = round(time.time() - t0, 2)
            return d
        except (ValueError, json.JSONDecodeError) as e:
            log(f"  retry ({e}; exit {p.returncode}) {p.stderr[-200:]}")
            time.sleep(20 * (attempt + 1))
    raise RuntimeError("claude -p failed 4 times")


def num(pattern, text):
    m = re.search(pattern + r"\W*~?\s*([\d,]+)", text, re.I)
    return int(m.group(1).replace(",", "")) if m else None


def extract(text):
    blocks = re.findall(r"```[a-zA-Z0-9]*\n(.*?)```", text, re.S)
    code = max(blocks, key=len) if blocks else text
    return code, num(r"vblank budget", text), num(r"cycles used", text)


def one(model, cpu, name, rep, outdir, variant="warned"):
    d = outdir / (f"{cpu}-{name}-r{rep}" if variant == "warned" else f"{variant}-{cpu}-{name}-r{rep}")
    if (d / "final.json").exists():
        return json.loads((d / "final.json").read_text())
    d.mkdir(parents=True, exist_ok=True)
    rounds, code, res = [], None, None
    for rnd in range(MAX_FIX + 1):
        text = prompt(cpu, name, variant) if rnd == 0 else fix_prompt(cpu, name, code, res, variant)
        (d / f"round{rnd}-prompt.txt").write_text(text)
        a = ask(model, cpu, text, d)
        reply = a.get("result", "")
        (d / f"round{rnd}-reply.md").write_text(reply)
        code, budget, used = extract(reply)
        src = d / f"round{rnd}.{'s' if cpu == '6502' else 'asm'}"
        src.write_text(code)
        res = vbrun.run(cpu, name, src, f"{model}-{variant}-{cpu}-{name}-r{rep}-{rnd}")
        (d / f"round{rnd}-result.json").write_text(json.dumps(res, indent=1))
        rounds.append(dict(round=rnd, assembled=res["assembled"], ok=res.get("ok", False),
                           timing_ok=res.get("timing_ok"), memory_ok=res.get("memory_ok"),
                           work_end_max=res.get("work_end_max"), vbl_end=res.get("vbl_end"),
                           budget=budget, used=used, cost=a.get("total_cost_usd"), wall_s=a["wall_s"],
                           out_tokens=sum(u.get("outputTokens", 0) for u in a.get("modelUsage", {}).values())))
        log(f"{model} {cpu} {name} r{rep} round {rnd}: "
            + ("no assemble" if not res["assembled"] else
               f"ok={res['ok']} timing={res['timing_ok']} memory={res['memory_ok']} end {res.get('work_end_max')}/{res.get('vbl_end')}")
            + f" budget {budget} used {used}")
        if res.get("ok"):
            break
    final = dict(model=model, variant=variant, cpu=cpu, task=name, rep=rep, rounds=rounds, ok=rounds[-1]["ok"],
                 first_try=rounds[0]["ok"], fix_rounds=len(rounds) - 1)
    (d / "final.json").write_text(json.dumps(final, indent=1))
    return final


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--model", default="claude-sonnet-5-5")
    ap.add_argument("--reps", type=int, default=3)
    ap.add_argument("--workers", type=int, default=4)
    ap.add_argument("--only")
    ap.add_argument("--cpus", default="6502,z80")
    ap.add_argument("--variant", default="warned", choices=["warned", "plain"])
    a = ap.parse_args()
    outdir = HERE / "ai-test" / a.model
    outdir.mkdir(parents=True, exist_ok=True)
    names = a.only.split(",") if a.only else ["pal", "row", "col", "big"]
    jobs = [(cpu, n, r) for r in range(1, a.reps + 1) for n in names for cpu in a.cpus.split(",")]
    with ThreadPoolExecutor(a.workers) as ex:
        results = list(ex.map(lambda j: one(a.model, *j, outdir, a.variant), jobs))
    with open(outdir / f"summary-{a.variant}-{'-'.join(names)}.jsonl", "w") as f:
        for r in results:
            f.write(json.dumps(r) + "\n")
    for cpu in a.cpus.split(","):
        rs = [r for r in results if r["cpu"] == cpu]
        print(f"{cpu}: first try {sum(r['first_try'] for r in rs)}/{len(rs)}, after fixes {sum(r['ok'] for r in rs)}/{len(rs)}")


if __name__ == "__main__":
    main()
