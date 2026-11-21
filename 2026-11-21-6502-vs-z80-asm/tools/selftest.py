#!/usr/bin/env python3
"""Checks the checker: for every task and CPU, builds the output record a
correct routine would leave (from the input record and the expected values),
and confirms check() accepts it and rejects a copy with one output bit flipped."""
import random
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import tasks  # noqa: E402


def ideal(name, cpu, v, inrec):
    regs, args, buf = bytearray(inrec[0:8]), bytearray(inrec[8:24]), bytearray(inrec[24:88])
    e = tasks.TASKS[name]["expect"](v)
    z = cpu == "z80"
    fl = 1 if z else 3

    def hl(x):
        regs[6], regs[7] = x >> 8, x & 0xFF

    def le(i, x):
        args[i], args[i + 1] = x & 0xFF, x >> 8

    if name in ("popcount", "isqrt16"):
        regs[0] = e["res"]
    elif name == "mul8":
        hl(e["res"]) if z else le(2, e["res"])
    elif name == "div16by8":
        if z: hl(e["q"]); regs[0] = e["rem"]
        else: le(3, e["q"]); args[5] = e["rem"]
    elif name == "utoa16":
        buf[v["off"]:v["off"] + 5] = e["digits"]
    elif name == "adds8":
        hl(e["res"]) if z else le(0, e["res"])
    elif name in ("cmp_s16", "box_overlap"):
        regs[fl] = (regs[fl] & 0xFE) | int(e["carry"])
    elif name == "memmove":
        src = bytes(buf)
        buf[v["dst"]:v["dst"] + v["n"]] = src[v["src"]:v["src"] + v["n"]]
    elif name == "sort_u8":
        buf[v["off"]:v["off"] + len(v["data"])] = bytes(sorted(v["data"]))
    return dict(regs=bytes(regs), args=bytes(args), buf=bytes(buf), cycles=0, status=0, stray=0)


def flip(name, cpu, out, v):
    """Corrupt one output bit (or, where the output may be empty, a buffer byte)."""
    o = dict(out)
    z = cpu == "z80"
    regs, args, buf = bytearray(o["regs"]), bytearray(o["args"]), bytearray(o["buf"])
    if name in ("popcount", "isqrt16"): regs[0] ^= 1
    elif name in ("mul8", "adds8"): (regs.__setitem__(7, regs[7] ^ 1) if z else args.__setitem__(2 if name == "mul8" else 0, args[2 if name == "mul8" else 0] ^ 1))
    elif name == "div16by8": (regs.__setitem__(0, regs[0] ^ 1) if z else args.__setitem__(5, args[5] ^ 1))
    elif name in ("cmp_s16", "box_overlap"): regs[1 if z else 3] ^= 1
    else: buf[(v.get("dst", v.get("off", 0)) + 63) % 64] ^= 1
    o.update(regs=bytes(regs), args=bytes(args), buf=bytes(buf))
    return o


bad = 0
for name in tasks.TASKS:
    vs = tasks.vectors(name)
    for cpu in ("6502", "z80"):
        r = random.Random(1)
        sample = random.Random(2).sample(range(len(vs)), min(800, len(vs)))
        for i in sample:
            inrec = tasks.pack(name, cpu, vs[i], r)
            good = ideal(name, cpu, vs[i], inrec)
            if tasks.check(name, cpu, vs[i], inrec, good) is not None:
                bad += 1; print("rejected a right answer", name, cpu, vs[i], tasks.check(name, cpu, vs[i], inrec, good))
                break
            if tasks.check(name, cpu, vs[i], inrec, flip(name, cpu, good, vs[i])) is None:
                bad += 1; print("accepted a wrong answer", name, cpu, vs[i])
                break
    print(f"{name}: {len(vs)} vectors, checker ok on both CPUs" if not bad else name)
print("selftest", "FAILED" if bad else "passed")
sys.exit(1 if bad else 0)
