"""The ten routines: what Claude is asked for, the test vectors, and the checks.

Each task has one spec per CPU (6502 on the NES, Z80 on the Master System),
a vector generator (seeded, so every routine sees the same cases), and an
expected-output function. A vector is a dict of task-level values; pack()
turns it into the 88-byte record the runners read (regs[8], args[16],
buf[64]) with every byte the task does not use filled with random junk, and
check() reads the 96-byte record the runner wrote back.
"""
import math
import random

NES_ARGS, NES_BUF = 0x0000, 0x0300
SMS_ARGS, SMS_BUF = 0xC000, 0xC100
BUFLEN = 64
CARRY = 0x01  # bit 0 in both P (6502) and F (Z80)

TASKS = {}


def task(name, spec6502, specz80, gen):
    def deco(fn):
        TASKS[name] = dict(name=name, spec={"6502": spec6502, "z80": specz80}, gen=gen, expect=fn)
        return fn
    return deco


def s16(v):
    return v - 0x10000 if v & 0x8000 else v


# --- vector generators ---------------------------------------------------------

def g_popcount(r):
    return [dict(a=a) for a in range(256) for _ in range(4)]


def g_mul8(r):
    return [dict(a=a, b=b) for a in range(256) for b in range(256)]


def g_div(r):
    edges = [0, 1, 2, 255, 256, 257, 32767, 32768, 65534, 65535]
    divs = [1, 2, 3, 7, 10, 127, 128, 129, 254, 255]
    v = [dict(n=n, d=d) for n in edges for d in divs]
    v += [dict(n=r.randrange(65536), d=r.randrange(1, 256)) for _ in range(8192)]
    return v


def g_utoa(r):
    return [dict(n=n, off=r.randrange(0, BUFLEN - 5)) for n in range(65536)]


def g_isqrt(r):
    return [dict(n=n) for n in range(65536)]


def g_adds8(r):
    pos = [0, 1, 0x7F, 0x80, 0xFF, 0x100, 0x17F, 0x180, 0x7FFF, 0x8000, 0xFF00, 0xFF7F, 0xFF80, 0xFFFF]
    pos += [r.randrange(65536) for _ in range(50)]
    return [dict(p=p, v=v) for p in pos for v in range(256)]


def g_cmp(r):
    edges = [0, 1, 0xFFFF, 0x7FFF, 0x8000, 0x8001, 0x7FFE, 0x00FF, 0x0100, 0xFF00, 0x00FE]
    v = [dict(a=a, b=b) for a in edges for b in edges]
    for _ in range(16384):
        a = r.randrange(65536)
        b = r.choice([r.randrange(65536), a ^ 0x8000, (a + r.randrange(-300, 300)) & 0xFFFF])
        v.append(dict(a=a, b=b))
    return v


def g_memmove(r):
    return [dict(src=s, dst=d, n=n) for s in range(32) for d in range(32) for n in range(33)]


def g_sort(r):
    v = []
    for i in range(3000):
        n = r.choice([0, 1, 2, 3, r.randrange(4, 17), r.randrange(17, 65)])
        off = r.randrange(0, BUFLEN - n + 1)
        kind = i % 4
        if kind == 0:
            data = [r.randrange(256) for _ in range(n)]
        elif kind == 1:
            data = [r.choice([0, 1, 127, 128, 255]) for _ in range(n)]
        elif kind == 2:
            data = sorted(r.randrange(256) for _ in range(n))
        else:
            data = sorted((r.randrange(256) for _ in range(n)), reverse=True)
        v.append(dict(off=off, data=data))
    return v


def g_box(r):
    v = []
    # touching edges and 8-bit overflow of x + w
    for ax, aw, bx, bw in [(0, 10, 10, 5), (10, 5, 0, 10), (0, 11, 10, 5), (250, 10, 2, 5), (250, 10, 252, 3),
                           (200, 100, 10, 20), (10, 20, 200, 100), (5, 0, 0, 20), (0, 255, 254, 255), (255, 1, 255, 1),
                           (128, 128, 0, 128), (0, 128, 128, 128), (0, 129, 128, 1), (240, 20, 0, 5)]:
        for ay, ah, by, bh in [(0, 8, 4, 8), (0, 8, 8, 8), (250, 10, 252, 3), (240, 20, 0, 5)]:
            v.append(dict(a=(ax, ay, aw, ah), b=(bx, by, bw, bh)))
            v.append(dict(a=(bx, by, bw, bh), b=(ax, ay, aw, ah)))
    for _ in range(16384):
        ax, ay = r.randrange(256), r.randrange(256)
        aw, ah = r.randrange(0, 64), r.randrange(0, 64)
        near = r.random() < 0.7
        bx = (ax + r.randrange(-70, 70)) & 0xFF if near else r.randrange(256)
        by = (ay + r.randrange(-70, 70)) & 0xFF if near else r.randrange(256)
        bw, bh = r.randrange(0, 64), r.randrange(0, 64)
        v.append(dict(a=(ax, ay, aw, ah), b=(bx, by, bw, bh)))
    return v


# --- specs ---------------------------------------------------------------------

COMMON = {
    "6502": "Registers A, X, Y and the flags not listed as outputs may hold anything on entry and on exit. "
            "Do not assume any flag state on entry (the carry may be set or clear). The NES CPU has no decimal mode.",
    "z80": "Registers and flags not listed as outputs may hold anything on entry and on exit, including IX, IY and "
           "the shadow registers. Do not assume any flag state on entry. Interrupts are off; leave them off.",
}


@task("popcount",
      "Count the 1 bits in A. Input: A. Output: A = number of set bits (0 to 8).",
      "Count the 1 bits in A. Input: A. Output: A = number of set bits (0 to 8).",
      g_popcount)
def e_popcount(v):
    return dict(res=bin(v["a"]).count("1"))


@task("mul8",
      "Unsigned 8 x 8 to 16-bit multiply. Inputs: zero page $00 and $01 (the two factors). "
      "Output: the 16-bit product in $02 (low byte) and $03 (high byte).",
      "Unsigned 8 x 8 to 16-bit multiply. Inputs: H and E (the two factors). Output: HL = the 16-bit product.",
      g_mul8)
def e_mul8(v):
    return dict(res=v["a"] * v["b"])


@task("div16by8",
      "Unsigned 16-bit by 8-bit division. Inputs: dividend in zero page $00 (low) and $01 (high), divisor in $02 "
      "(1 to 255, never 0). Outputs: 16-bit quotient in $03 (low) and $04 (high), remainder in $05.",
      "Unsigned 16-bit by 8-bit division. Inputs: dividend in HL, divisor in C (1 to 255, never 0). "
      "Outputs: HL = 16-bit quotient, A = remainder.",
      g_div)
def e_div(v):
    return dict(q=v["n"] // v["d"], rem=v["n"] % v["d"])


@task("utoa16",
      "Convert an unsigned 16-bit number to 5 ASCII decimal digits with leading zeros (12345 -> \"12345\", "
      "7 -> \"00007\"). Inputs: the number in zero page $00 (low) and $01 (high), a pointer to the output in $02 "
      "(low) and $03 (high). Output: 5 bytes '0'..'9' written at the pointer, most significant digit first. "
      "Write nothing else there (no terminator).",
      "Convert an unsigned 16-bit number to 5 ASCII decimal digits with leading zeros (12345 -> \"12345\", "
      "7 -> \"00007\"). Inputs: the number in HL, a pointer to the output in DE. Output: 5 bytes '0'..'9' written "
      "at DE, most significant digit first. Write nothing else there (no terminator).",
      g_utoa)
def e_utoa(v):
    return dict(digits=b"%05d" % v["n"])


@task("isqrt16",
      "Integer square root: floor(sqrt(n)) of an unsigned 16-bit n. Input: n in zero page $00 (low) and $01 (high). "
      "Output: A = the root (0 to 255).",
      "Integer square root: floor(sqrt(n)) of an unsigned 16-bit n. Input: n in HL. Output: A = the root (0 to 255).",
      g_isqrt)
def e_isqrt(v):
    return dict(res=math.isqrt(v["n"]))


@task("adds8",
      "Add a signed 8-bit velocity to a 16-bit position (two's complement, wrapping at 16 bits). Inputs: position "
      "in zero page $00 (low) and $01 (high), velocity in A (-128 to 127). Output: the new position in $00/$01.",
      "Add a signed 8-bit velocity to a 16-bit position (two's complement, wrapping at 16 bits). Inputs: position "
      "in HL, velocity in A (-128 to 127). Output: HL = the new position.",
      g_adds8)
def e_adds8(v):
    vel = v["v"] - 256 if v["v"] & 0x80 else v["v"]
    return dict(res=(v["p"] + vel) & 0xFFFF)


@task("cmp_s16",
      "Signed 16-bit less-than. Inputs: a in zero page $00 (low) and $01 (high), b in $02 (low) and $03 (high), both "
      "two's complement (-32768 to 32767). Output: carry flag set if a < b, clear otherwise.",
      "Signed 16-bit less-than. Inputs: a in HL, b in DE, both two's complement (-32768 to 32767). "
      "Output: carry flag set if a < b, clear otherwise.",
      g_cmp)
def e_cmp(v):
    return dict(carry=s16(v["a"]) < s16(v["b"]))


@task("memmove",
      "Copy count bytes from src to dst, correct when the two areas overlap in either direction (like C memmove). "
      "Inputs: src pointer in zero page $00 (low) and $01 (high), dst pointer in $02 (low) and $03 (high), count in X "
      "(0 to 255; 0 copies nothing). Output: the copied bytes at dst. Do not write outside dst..dst+count-1.",
      "Copy count bytes from src to dst, correct when the two areas overlap in either direction (like C memmove). "
      "Inputs: src in HL, dst in DE, count in BC (0 to 65535; 0 copies nothing). Output: the copied bytes at dst. "
      "Do not write outside dst..dst+count-1.",
      g_memmove)
def e_memmove(v):
    return {}


@task("sort_u8",
      "Sort an array of unsigned bytes in place, ascending. Inputs: pointer in zero page $00 (low) and $01 (high), "
      "length in X (0 to 255). Output: the sorted array in place.",
      "Sort an array of unsigned bytes in place, ascending. Inputs: pointer in HL, length in B (0 to 255). "
      "Output: the sorted array in place.",
      g_sort)
def e_sort(v):
    return {}


@task("box_overlap",
      "Axis-aligned box overlap test for sprites. Each box is 4 bytes: x, y, width, height (unsigned). Box A is in zero "
      "page $00-$03, box B in $04-$07. They overlap when ax < bx + bw and bx < ax + aw and ay < by + bh and "
      "by < ay + ah, with the sums computed without 8-bit wraparound (x + width can exceed 255). A box with width or "
      "height 0 overlaps nothing. Output: carry flag set if they overlap, clear if not.",
      "Axis-aligned box overlap test for sprites. Each box is 4 bytes: x, y, width, height (unsigned). HL points to "
      "8 bytes: box A then box B. They overlap when ax < bx + bw and bx < ax + aw and ay < by + bh and "
      "by < ay + ah, with the sums computed without 8-bit wraparound (x + width can exceed 255). A box with width or "
      "height 0 overlaps nothing. Output: carry flag set if they overlap, clear if not.",
      g_box)
def e_box(v):
    ax, ay, aw, ah = v["a"]
    bx, by, bw, bh = v["b"]
    # The spec says a 0-size box overlaps nothing. The first version of this
    # check used only the four inequalities, which call a 0-width box inside a
    # wider one overlapping: 63 vectors disagreed with the spec, and all 12
    # first tries failed on exactly those 63 (see README, "My checker was wrong").
    if 0 in (aw, ah, bw, bh):
        return dict(carry=False)
    return dict(carry=ax < bx + bw and bx < ax + aw and ay < by + bh and by < ay + ah)


# --- packing and checking ------------------------------------------------------

def vectors(name, seed=2026):
    r = random.Random(f"{name}-{seed}")
    return TASKS[name]["gen"](r)


def pack(name, cpu, v, r):
    regs = bytearray(r.randbytes(8))
    args = bytearray(r.randbytes(16))
    buf = bytearray(r.randbytes(BUFLEN))
    base = NES_BUF if cpu == "6502" else SMS_BUF
    regs[3 if cpu == "6502" else 1] = r.choice([0x00, 0x01, 0xFF, 0xFE, r.randrange(256)])  # flags
    z = cpu == "z80"
    A, F, B, C, D, E, H, L = range(8)

    def w16(arr, i, val):
        arr[i] = val & 0xFF
        arr[i + 1] = val >> 8

    def hl(val):
        regs[H], regs[L] = val >> 8, val & 0xFF

    def de(val):
        regs[D], regs[E] = val >> 8, val & 0xFF

    if name == "popcount":
        regs[0] = v["a"]
    elif name == "mul8":
        if z: regs[H], regs[E] = v["a"], v["b"]
        else: args[0], args[1] = v["a"], v["b"]
    elif name == "div16by8":
        if z: hl(v["n"]); regs[C] = v["d"]
        else: w16(args, 0, v["n"]); args[2] = v["d"]
    elif name == "utoa16":
        if z: hl(v["n"]); de(base + v["off"])
        else: w16(args, 0, v["n"]); w16(args, 2, base + v["off"])
    elif name == "isqrt16":
        if z: hl(v["n"])
        else: w16(args, 0, v["n"])
    elif name == "adds8":
        regs[0] = v["v"]
        if z: hl(v["p"])
        else: w16(args, 0, v["p"])
    elif name == "cmp_s16":
        if z: hl(v["a"]); de(v["b"])
        else: w16(args, 0, v["a"]); w16(args, 2, v["b"])
    elif name == "memmove":
        if z: hl(base + v["src"]); de(base + v["dst"]); regs[B], regs[C] = 0, v["n"]
        else: w16(args, 0, base + v["src"]); w16(args, 2, base + v["dst"]); regs[1] = v["n"]
    elif name == "sort_u8":
        buf[v["off"]:v["off"] + len(v["data"])] = bytes(v["data"])
        if z: hl(base + v["off"]); regs[B] = len(v["data"])
        else: w16(args, 0, base + v["off"]); regs[1] = len(v["data"])
    elif name == "box_overlap":
        if z: args[0:8] = bytes(v["a"] + v["b"]); hl(SMS_ARGS)
        else: args[0:8] = bytes(v["a"] + v["b"])
    return bytes(regs + args + buf)


def unpack_out(rec):
    return dict(regs=rec[0:8], args=rec[8:24], buf=rec[24:88], cycles=int.from_bytes(rec[88:92], "little"),
                status=rec[92], stray=(rec[93] << 8) | rec[94])


def check(name, cpu, v, inrec, out):
    """Returns None if right, else a short description of what was wrong."""
    st = out["status"]
    if st & 1: return "timeout (no return)"
    if st & 2: return "stack pointer changed"
    if st & 4: return f"wrote RAM outside its area (${out['stray']:04X})"
    exp = TASKS[name]["expect"](v)
    z = cpu == "z80"
    regs, args, buf = out["regs"], out["args"], out["buf"]
    in_buf = bytearray(inrec[24:88])
    flags = regs[1] if z else regs[3]
    hlv = (regs[6] << 8) | regs[7]
    a = regs[0]

    def le(i):
        return args[i] | (args[i + 1] << 8)

    def buf_unchanged():
        return None if bytes(buf) == bytes(in_buf) else "changed the buffer"

    if name in ("popcount", "isqrt16"):
        return None if a == exp["res"] else f"A={a} want {exp['res']}"
    if name == "mul8":
        got = hlv if z else le(2)
        return None if got == exp["res"] else f"product {got} want {exp['res']}"
    if name == "div16by8":
        q, rem = (hlv, a) if z else (le(3), args[5])
        return None if (q, rem) == (exp["q"], exp["rem"]) else f"q={q} r={rem} want q={exp['q']} r={exp['rem']}"
    if name == "utoa16":
        want = bytearray(in_buf)
        want[v["off"]:v["off"] + 5] = exp["digits"]
        if bytes(buf) == bytes(want): return None
        return f"wrote {bytes(buf[v['off']:v['off'] + 6])!r} want {exp['digits']!r}" if buf[v["off"]:v["off"] + 5] != exp["digits"] else "wrote past the 5 digits"
    if name == "adds8":
        got = hlv if z else le(0)
        return None if got == exp["res"] else f"position ${got:04X} want ${exp['res']:04X}"
    if name in ("cmp_s16", "box_overlap"):
        c = bool(flags & CARRY)
        return None if c == exp["carry"] else f"carry {int(c)} want {int(exp['carry'])}"
    if name == "memmove":
        want = bytearray(in_buf)
        s, d, n = v["src"], v["dst"], v["n"]
        want[d:d + n] = in_buf[s:s + n]
        return None if bytes(buf) == bytes(want) else "wrong bytes"
    if name == "sort_u8":
        want = bytearray(in_buf)
        o, data = v["off"], v["data"]
        want[o:o + len(data)] = bytes(sorted(data))
        return None if bytes(buf) == bytes(want) else "not sorted (or touched other bytes)"
    raise KeyError(name)
