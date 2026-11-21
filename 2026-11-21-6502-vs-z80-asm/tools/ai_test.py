#!/usr/bin/env python3
"""Asks Claude for each routine on each CPU, runs it on the emulator, and
gives it up to two fix rounds with the emulator's report.

  1. `claude -p` (tools off, no CLAUDE.md, no settings) gets the spec and
     replies with one code block plus a "Worst-case cycles: N" line.
  2. tools/asmrun.py assembles it into the harness ROM and runs every test
     vector on FCEUmm (6502) or Genesis Plus GX (Z80).
  3. If it did not assemble or got any vector wrong, a new prompt carries the
     spec, the previous code and the report (assembler output, or the kinds
     of failures and four failing vectors). At most two such rounds.

Results land in ai-test/<model>/<task>-<cpu>-r<rep>/ and ai-test/<model>/summary.jsonl.
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
import asmrun  # noqa: E402
import tasks  # noqa: E402

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


def prompt(cpu, name):
    spec = tasks.TASKS[name]["spec"][cpu]
    if cpu == "6502":
        return f"""Write a 6502 routine for the NES (NTSC, 2A03 CPU) in ca65 syntax.

Routine label: `{name}`. It is called with JSR and must return with RTS.

Task: {spec}

Rules:
- {tasks.COMMON['6502']}
- Zero page $00-$0F is yours: the inputs live there and you may use any of it as scratch. Use the stack if you need more. Do not write any other RAM, except where the task says to write its output.
- The code runs from ROM, so no self-modifying code. Tables in ROM are fine (after the code, with .byte).
- Your file is included into the harness's CODE segment: no .segment, .org, header or vectors. Prefix your labels with `{name}_` or use @cheap local labels.

Reply with the whole routine in one ```asm code block. After the block, write one line: `Worst-case cycles: N`, counting the JSR and the RTS."""
    return f"""Write a Z80 routine for the Sega Master System (NTSC) in sjasmplus syntax.

Routine label: `{name}`. It is called with CALL and must return with RET.

Task: {spec}

Rules:
- {tasks.COMMON['z80']}
- RAM $C000-$C00F is yours as scratch. Use the stack if you need more. Do not write any other RAM, except where the task says to write its output.
- The code runs from ROM, so no self-modifying code. Tables in ROM are fine (after the code, with DB).
- Your file is included into the harness after its code: no ORG, OUTPUT, DEVICE or header. Prefix your labels with `{name}_` or use local labels (.label).

Reply with the whole routine in one ```asm code block. After the block, write one line: `Worst-case cycles: N`, counting the CALL and the RET."""


def fix_prompt(cpu, name, code, res):
    if not res["assembled"]:
        report = "It did not assemble. Assembler output:\n```\n" + res["asm_log"][-2500:] + "\n```"
    else:
        kinds = "\n".join(f"- {c} x {k}" for k, c in res["fail_kinds"].items())
        ex = "\n".join(f"- inputs {fmt(f['vector'])}: {f['got']}" for f in res["first_fails"][:4])
        report = (f"It assembled and ran on an emulator against {res['vectors']} test vectors: {res['passed']} right, "
                  f"{res['failed']} wrong.\nKinds of failure:\n{kinds}\nExamples:\n{ex}")
    return (prompt(cpu, name) + "\n\nYour previous answer was:\n```asm\n" + code + "\n```\n\n" + report +
            "\n\nFix the routine. Reply the same way: the whole routine in one code block, then the cycles line.")


def fmt(v):
    return ", ".join(f"{k}={list(x) if isinstance(x, (list, tuple)) else x}" for k, x in v.items())


def ask(model, cpu, text, cwd):
    for attempt in range(4):
        t0 = time.time()
        p = subprocess.run(["claude", "-p", "--model", model, "--system-prompt", SYSTEM[cpu], "--tools", "",
                            "--strict-mcp-config", "--setting-sources", "", "--output-format", "json",
                            "--no-session-persistence"],
                           input=text, capture_output=True, text=True, env=ENV, cwd=cwd, timeout=900)
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


def extract(text):
    blocks = re.findall(r"```[a-zA-Z0-9]*\n(.*?)```", text, re.S)
    code = max(blocks, key=len) if blocks else text
    m = re.search(r"worst[- ]case cycles\W*~?\s*([\d,]+)", text, re.I)
    claim = int(m.group(1).replace(",", "")) if m else None
    return code, claim


def one(model, cpu, name, rep, outdir):
    d = outdir / f"{name}-{cpu}-r{rep}"
    if (d / "final.json").exists():
        return json.loads((d / "final.json").read_text())
    d.mkdir(parents=True, exist_ok=True)
    rounds, code, res = [], None, None
    for rnd in range(MAX_FIX + 1):
        text = prompt(cpu, name) if rnd == 0 else fix_prompt(cpu, name, code, res)
        (d / f"round{rnd}-prompt.txt").write_text(text)
        a = ask(model, cpu, text, d)
        reply = a.get("result", "")
        (d / f"round{rnd}-reply.md").write_text(reply)
        code, claim = extract(reply)
        src = d / f"round{rnd}.{'s' if cpu == '6502' else 'asm'}"
        src.write_text(code)
        res = asmrun.run(cpu, name, src, asmrun.WORK / "runs" / f"{model}-{name}-{cpu}-r{rep}-{rnd}")
        (d / f"round{rnd}-result.json").write_text(json.dumps(res, indent=1, default=str))
        rounds.append(dict(round=rnd, assembled=res["assembled"], passed=res.get("passed"), vectors=res.get("vectors"),
                           ok=res.get("ok", False), fail_kinds=res.get("fail_kinds"), claim=claim,
                           cycles_max=res.get("cycles_max"), cycles_median=res.get("cycles_median"),
                           cost=a.get("total_cost_usd"), wall_s=a["wall_s"],
                           out_tokens=sum(u.get("outputTokens", 0) for u in a.get("modelUsage", {}).values())))
        log(f"{model} {name} {cpu} r{rep} round {rnd}: "
            + ("no assemble" if not res["assembled"] else f"{res['passed']}/{res['vectors']}")
            + f" claim {claim} max {res.get('cycles_max')}")
        if res.get("ok"):
            break
    final = dict(model=model, cpu=cpu, task=name, rep=rep, rounds=rounds, ok=rounds[-1]["ok"],
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
    a = ap.parse_args()
    outdir = HERE / "ai-test" / a.model
    outdir.mkdir(parents=True, exist_ok=True)
    names = a.only.split(",") if a.only else list(tasks.TASKS)
    jobs = [(cpu, n, r) for r in range(1, a.reps + 1) for n in names for cpu in a.cpus.split(",")]
    with ThreadPoolExecutor(a.workers) as ex:
        results = list(ex.map(lambda j: one(a.model, *j, outdir), jobs))
    with open(outdir / "summary.jsonl", "w") as f:
        for r in results:
            f.write(json.dumps(r) + "\n")
    for cpu in a.cpus.split(","):
        rs = [r for r in results if r["cpu"] == cpu]
        print(f"{cpu}: first try {sum(r['first_try'] for r in rs)}/{len(rs)}, after fixes {sum(r['ok'] for r in rs)}/{len(rs)}")


if __name__ == "__main__":
    main()
