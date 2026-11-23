#!/usr/bin/env python3
"""Build one VBlank routine into the harness for each of its 3 cases, run it
headless, and check it.

  vbrun.py 6502|z80 TASK routine-file [--out NAME] [--json] [--save-ref]

For each case: assemble (ca65/ld65 or sjasmplus), run 30 frames on FCEUmm
(nesvb) or Genesis Plus GX (smsvb), then check
  - memory: the target area of the video memory holds the expected bytes,
  - same as reference: the whole video memory dump and the last frame's
    picture equal those of my reference routine (validate/ref-*), which
    catches stray writes, wrong scroll or PPUCTRL, and broken pictures,
  - timing: in every frame from 15 on (the NMI is on from frame 10), the handler returned, and the last
    PPU/VDP write (plus the OAM DMA's 513 cycles) ended before vertical
    blank did.
--save-ref stores the run as the reference (used for my own routines only).
Everything runs inside the gcc:14 Docker image with no network.
"""
import argparse
import json
import os
import shutil
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
SJASM = Path(os.environ.get("SJASM_WORK", HOME / ".cache/asmtest"))
WORK = Path(os.environ.get("WORK", HOME / ".cache/vblankasm"))
FRAMES, FROM = 40, 15


def docker(cmd, workdir, timeout=300):
    args = ["docker", "run", "--rm", "--network", "none", "--user", f"{os.getuid()}:{os.getgid()}",
            "-v", f"{WORK}:/w", "-v", f"{NESW}:/n", "-v", f"{SMSW}:/s", "-v", f"{CC65}:/c", "-v", f"{SJASM}:/j",
            "-w", f"/w/{workdir.relative_to(WORK)}", "gcc:14", "sh", "-c", cmd]
    return subprocess.run(args, capture_output=True, text=True, timeout=timeout)


def build_and_run(cpu, task, case, src, d):
    if d.exists():
        shutil.rmtree(d)
    d.mkdir(parents=True)
    name = "vblank_update"
    if cpu == "6502":
        shutil.copy(src, d / "routine.s")
        (d / "harness.s").write_text((HERE / "harness/nes.s").read_text().replace("ROUTINE", name))
        shutil.copy(HERE / "harness/nes.cfg", d / "nes.cfg")
        (d / "chr.bin").write_bytes(tasks.chr_bin())
        (d / "case.s").write_text(tasks.include_file(cpu, task, case))
        p = docker("/c/cc65/bin/ca65 -g harness.s -o harness.o -l harness.lst && "
                   "/c/cc65/bin/ld65 -C nes.cfg harness.o -o rom.nes -Ln labels.txt", d)
        log = (p.stdout + p.stderr).strip()
        if p.returncode or not (d / "rom.nes").exists():
            return dict(assembled=False, asm_log=log)
        labels = dict(reversed(line.split()[1:3]) for line in (d / "labels.txt").read_text().splitlines())
        nmi, nexit = labels[".nmi"][2:], labels[".nmi_exit"][2:]
        p = docker(f"/w/bin/nesvb /n/fceumm/fceumm_libretro.so rom.nes {nmi} {nexit} out --frames {FRAMES} --from {FROM}", d)
    else:
        shutil.copy(src, d / "routine.asm")
        (d / "harness.asm").write_text((HERE / "harness/sms.asm").read_text().replace("ROUTINE", name))
        (d / "tiles.bin").write_bytes(tasks.tiles_bin())
        (d / "case.asm").write_text(tasks.include_file(cpu, task, case))
        p = docker("/j/sjasmplus/sjasmplus --nologo --msg=war --lst=harness.lst harness.asm", d)
        log = (p.stdout + p.stderr).strip()
        if p.returncode or not (d / "rom.sms").exists():
            return dict(assembled=False, asm_log=log)
        p = docker(f"/w/bin/smsvb /s/gpgx/genesis_plus_gx_libretro.so rom.sms out --frames {FRAMES} --from {FROM}", d)
    if p.returncode or not (d / "out.json").exists():
        return dict(assembled=True, ran=False, run_log=(p.stdout + p.stderr)[-1000:])
    return dict(assembled=True, ran=True, asm_log=log)


def analyse(cpu, task, case, d, ref_dir):
    j = json.loads((d / "out.json").read_text())
    fs = [f for f in j["frames"] if f["f"] >= FROM]
    dump = (d / "out-vram.bin").read_bytes()
    r = dict(case=case)
    # memory
    bad = 0
    for off, want in tasks.expected(cpu, task, case):
        bad += sum(1 for i, b in enumerate(want) if dump[off + i] != b)
    r["wrong_bytes"] = bad
    # reference
    if (ref_dir / "out-vram.bin").exists():
        refd = (ref_dir / "out-vram.bin").read_bytes()
        r["dump_diff"] = sum(1 for a, b in zip(dump, refd) if a != b)
        r["picture_diff"] = sum(1 for a, b in zip((d / "out.ppm").read_bytes(), (ref_dir / "out.ppm").read_bytes())
                                if a != b) // 1
    # timing
    r["frames"] = len(fs)
    r["entered"] = sum(f["entered"] for f in fs)
    r["returned"] = sum(f["exited"] for f in fs)
    r["reentered"] = sum(f["reentered"] for f in fs)
    if cpu == "6502":
        r["vbl_end"] = min(f["vbl_end"] for f in fs)
        r["work_end_max"] = max(f["work_end"] for f in fs)
        r["late_frames"] = sum(1 for f in fs if f["work_end"] > f["vbl_end"])
        r["writes"] = max(f["writes"] for f in fs)
        r["rmw_ppu"] = sum(f["rmw_ppu"] for f in fs)
    else:
        r["vbl_end"] = j["vbl_end"]
        r["entry"] = min(f["entry"] for f in fs)
        r["work_end_max"] = max(f["last_write"] for f in fs)
        r["late_frames"] = sum(1 for f in fs if f["last_write"] > j["vbl_end"] or f["active"])
        r["active_writes"] = max(f["active"] for f in fs)
        r["fast26"] = max(f["fast26"] for f in fs)
        r["writes"] = max(f["writes"] for f in fs)
    r["timing_ok"] = r["late_frames"] == 0 and r["entered"] == r["returned"] == len(fs) and not r["reentered"]
    r["memory_ok"] = bad == 0 and r.get("dump_diff", 0) == 0 and r.get("picture_diff", 0) == 0
    r["ok"] = r["timing_ok"] and r["memory_ok"]
    return r


def run(cpu, task, src, name, save_ref=False):
    out = dict(cpu=cpu, task=task, cases=[])
    for case in tasks.CASES[(cpu, task)]:
        d = WORK / "runs" / name / f"case{case}"
        ref_dir = WORK / "ref" / f"{cpu}-{task}-{case}"
        b = build_and_run(cpu, task, case, Path(src).resolve(), d)
        if not b["assembled"]:
            out.update(assembled=False, asm_log=b["asm_log"][-3000:], ok=False)
            return out
        if not b["ran"]:
            out["cases"].append(dict(case=case, ok=False, error="runner failed", log=b["run_log"]))
            continue
        if save_ref:
            ref_dir.mkdir(parents=True, exist_ok=True)
            for f in ("out-vram.bin", "out.ppm", "out.json"):
                shutil.copy(d / f, ref_dir / f)
        out["cases"].append(analyse(cpu, task, case, d, ref_dir))
    out["assembled"] = True
    out["ok"] = all(c.get("ok") for c in out["cases"])
    out["timing_ok"] = all(c.get("timing_ok") for c in out["cases"])
    out["memory_ok"] = all(c.get("memory_ok") for c in out["cases"])
    cs = [c for c in out["cases"] if "work_end_max" in c]
    if cs:
        out["work_end_max"] = max(c["work_end_max"] for c in cs)
        out["vbl_end"] = min(c["vbl_end"] for c in cs)
    return out


def report(res):
    """The plain-text report a fix round gets."""
    if not res["assembled"]:
        return "It did not assemble. Assembler output:\n```\n" + res["asm_log"][-2500:] + "\n```"
    lines = [f"It assembled and ran on an emulator in {len(res['cases'])} test cases, {FRAMES - FROM} frames checked in each."]
    for c in res["cases"]:
        if "error" in c:
            lines.append(f"- case {c['case']}: the emulator run failed")
            continue
        p = []
        if c["wrong_bytes"]:
            p.append(f"{c['wrong_bytes']} bytes of the destination wrong")
        elif c.get("dump_diff"):
            p.append(f"destination right, but {c['dump_diff']} other bytes of video memory differ from a correct routine's")
        if c.get("picture_diff") and not c["wrong_bytes"] and not c.get("dump_diff"):
            p.append("video memory right, but the picture drawn differs from a correct routine's (PPU state left wrong?)")
        if c["returned"] < c["frames"] or c["reentered"]:
            p.append(f"the handler returned in {c['returned']} of {c['frames']} frames")
        if c["late_frames"]:
            unit = "CPU cycles after the NMI handler started" if res["cpu"] == "6502" else \
                "Z80 cycles after the start of vertical blank (line 192)"
            p.append(f"in {c['late_frames']} of {c['frames']} frames the last write ended at {c['work_end_max']} "
                     f"{unit}; vertical blank ended at {c['vbl_end']}")
        lines.append(f"- case {c['case']}: " + ("passed" if c.get("ok") else "; ".join(p)))
    return "\n".join(lines)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("cpu", choices=["6502", "z80"])
    ap.add_argument("task", choices=["pal", "row", "col", "big", "huge"])
    ap.add_argument("src")
    ap.add_argument("--out")
    ap.add_argument("--json", action="store_true")
    ap.add_argument("--save-ref", action="store_true")
    a = ap.parse_args()
    res = run(a.cpu, a.task, a.src, a.out or f"{a.cpu}-{a.task}", a.save_ref)
    if a.json:
        print(json.dumps(res, indent=1))
    else:
        print(json.dumps({k: v for k, v in res.items() if k != "cases"}))
        for c in res["cases"]:
            print(" ", c)
        print(report(res))


if __name__ == "__main__":
    main()
