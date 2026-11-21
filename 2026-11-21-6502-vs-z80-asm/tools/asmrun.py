#!/usr/bin/env python3
"""Assemble one routine into its harness ROM, run every test vector on the
emulator core, and check the results.

  asmrun.py 6502|z80 TASK routine-file [--out DIR] [--json]

6502: ca65/ld65 (cc65 from the 2026-11-17 project), runs on FCEUmm (nesrun).
Z80: sjasmplus, runs on Genesis Plus GX in Master System mode (smsrun).
Everything runs inside the gcc:14 Docker image with no network. Prints a
summary; with --json, one JSON object (used by ai_test.py).
"""
import argparse
import json
import os
import random
import re
import shutil
import statistics
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(HERE / "tools"))
import tasks  # noqa: E402

HOME = Path.home()
NESW = Path(os.environ.get("NESW", HOME / ".cache/nesprof"))
SMSW = Path(os.environ.get("SMSW", HOME / ".cache/smsvdp"))
CC65 = Path(os.environ.get("CC65_WORK", HOME / ".cache/nessprites"))
WORK = Path(os.environ.get("WORK", HOME / ".cache/asmtest"))
INREC, OUTREC = 88, 96
CLOCK = {"6502": 1789773, "z80": 3579545}  # NTSC NES CPU, NTSC Master System Z80 (Hz)


def docker(cmd, workdir, timeout=900):
    args = ["docker", "run", "--rm", "--network", "none", "--user", f"{os.getuid()}:{os.getgid()}",
            "-v", f"{WORK}:/w", "-v", f"{NESW}:/n", "-v", f"{SMSW}:/s", "-v", f"{CC65}:/c",
            "-w", f"/w/{workdir.relative_to(WORK)}", "gcc:14", "sh", "-c", cmd]
    return subprocess.run(args, capture_output=True, text=True, timeout=timeout)


def vector_file(cpu, name):
    """The packed vectors for one task and CPU, cached in WORK/vectors."""
    d = WORK / "vectors"
    d.mkdir(parents=True, exist_ok=True)
    p = d / f"{name}-{cpu}.bin"
    vs = tasks.vectors(name)
    if not p.exists():
        r = random.Random(f"pack-{name}-{cpu}")
        p.write_bytes(b"".join(tasks.pack(name, cpu, v, r) for v in vs))
    return p, vs


def build(cpu, name, src, d):
    if d.exists():
        shutil.rmtree(d)
    d.mkdir(parents=True)
    if cpu == "6502":
        shutil.copy(src, d / "routine.s")
        (d / "harness.s").write_text((HERE / "harness/nes.s").read_text().replace("ROUTINE", name))
        shutil.copy(HERE / "harness/nes.cfg", d / "nes.cfg")
        p = docker("/c/cc65/bin/ca65 -g harness.s -o harness.o -l harness.lst && "
                   "/c/cc65/bin/ld65 -C nes.cfg harness.o -o rom.nes -Ln labels.txt", d)
    else:
        shutil.copy(src, d / "routine.asm")
        (d / "harness.asm").write_text((HERE / "harness/sms.asm").read_text().replace("ROUTINE", name))
        p = docker("/w/sjasmplus/sjasmplus --nologo --msg=war --lst=harness.lst harness.asm", d)
    log = (p.stdout + p.stderr).strip()
    rom = d / ("rom.nes" if cpu == "6502" else "rom.sms")
    if p.returncode != 0 or not rom.exists():
        return None, log
    return rom, log


def run(cpu, name, src, d):
    rom, log = build(cpu, name, src, d)
    res = dict(cpu=cpu, task=name, assembled=rom is not None, asm_log=log[-3000:])
    if rom is None:
        return res
    vfile, vs = vector_file(cpu, name)
    shutil.copy(vfile, d / "in.bin")
    if cpu == "6502":
        labels = dict(reversed(line.split()[1:3]) for line in (d / "labels.txt").read_text().splitlines())
        call, after = labels[".call"][2:], labels[".after"][2:]
        p = docker(f"/w/bin/nesrun /n/fceumm/fceumm_libretro.so rom.nes {call} {after} in.bin out.bin", d)
    else:
        p = docker("/w/bin/smsrun /s/gpgx/genesis_plus_gx_libretro.so rom.sms in.bin out.bin", d)
    res["runner"] = (p.stdout + p.stderr).strip()[-500:]
    out = (d / "out.bin").read_bytes() if (d / "out.bin").exists() else b""
    inb = (d / "in.bin").read_bytes()
    n = len(out) // OUTREC
    fails, cycles_ok, reasons = [], [], {}
    for i in range(n):
        o = tasks.unpack_out(out[i * OUTREC:(i + 1) * OUTREC])
        why = tasks.check(name, cpu, vs[i], inb[i * INREC:(i + 1) * INREC], o)
        if why is None:
            cycles_ok.append(o["cycles"])
        else:
            kind = re.sub(r"[-\d$A-F]{2,}|\d+", "#", why) if not why.startswith("wrote RAM") else "wrote RAM outside its area"
            reasons[kind] = reasons.get(kind, 0) + 1
            if len(fails) < 8:
                fails.append(dict(vector=vs[i], got=why))
    res.update(vectors=len(vs), ran=n, passed=len(cycles_ok), failed=n - len(cycles_ok) + (len(vs) - n),
               fail_kinds=reasons, first_fails=fails)
    if cycles_ok:
        res.update(cycles_min=min(cycles_ok), cycles_median=statistics.median(cycles_ok), cycles_max=max(cycles_ok),
                   us_max=round(max(cycles_ok) / CLOCK[cpu] * 1e6, 1))
    (d / "out.bin").unlink(missing_ok=True)
    (d / "in.bin").unlink(missing_ok=True)
    res["ok"] = res["assembled"] and res["passed"] == len(vs)
    return res


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("cpu", choices=["6502", "z80"])
    ap.add_argument("task", choices=list(tasks.TASKS))
    ap.add_argument("src")
    ap.add_argument("--out", default=None, help="build folder under the cache (default: runs/<task>-<cpu>)")
    ap.add_argument("--json", action="store_true")
    a = ap.parse_args()
    d = WORK / "runs" / (a.out or f"{a.task}-{a.cpu}")
    res = run(a.cpu, a.task, Path(a.src).resolve(), d)
    if a.json:
        print(json.dumps(res, default=str))
        return
    if not res["assembled"]:
        print("did not assemble:\n" + res["asm_log"])
        return
    print(f"{a.task} on {a.cpu}: {res['passed']}/{res['vectors']} vectors right")
    if res.get("cycles_max") is not None:
        print(f"cycles (call + return included) min {res['cycles_min']} median {res['cycles_median']} max {res['cycles_max']}"
              f" ({res['us_max']} us at worst)")
    for k, c in res["fail_kinds"].items():
        print(f"  {c} x {k}")
    for f in res["first_fails"][:4]:
        print("  e.g.", f)
    print(res["runner"])


if __name__ == "__main__":
    main()
