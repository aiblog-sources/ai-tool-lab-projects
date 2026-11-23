"""The four VBlank update tasks per CPU: what Claude is asked for, the test
cases, and the expected video memory.

Every task runs in 3 cases (different row, column, address or data), each
built into its own ROM. The data is seeded, so every routine sees the same
cases. Files the harness includes (chr.bin, tiles.bin, case.s, case.asm) are
generated here.
"""
import random

NES_CONTRACT = """Your routine is called once per frame from the NMI handler of an NTSC NES game, with rendering on. \
The handler has already saved A, X and Y and does nothing else: it calls your routine with JSR and returns with RTI \
right after. Before the NMI, PPUCTRL ($2000) was $80 (NMI on, base nametable $2000, VRAM increment 1, sprite and \
background patterns at $0000) and PPUMASK ($2001) was $1E. The cartridge uses vertical mirroring. All PPU and OAM DMA \
work must be finished inside vertical blank. Before returning, leave PPUCTRL at $80 and the scroll at X=0, Y=0, so the \
next frame is drawn from nametable $2000 unscrolled."""

SMS_CONTRACT = """Your routine is called once per frame from the VBlank (frame) interrupt handler of an NTSC Sega \
Master System game, with the display on (256x192 screen, mode 4, name table at VRAM $3800). The handler has already \
saved every register (main and shadow set) and read the VDP control port $BF, which acknowledged the interrupt and \
reset the control port's byte latch; it calls your routine with CALL and returns with EI and RETI right after. All VDP \
writes must be finished inside vertical blank, before the active display starts again. Do not change VDP registers."""

# The "plain" condition: the same facts without the timing and scroll requirements spelled out.
NES_PLAIN = """Your routine is called once per frame from the NMI handler of an NTSC NES game, with rendering on. \
The handler has already saved A, X and Y and does nothing else: it calls your routine with JSR and returns with RTI \
right after. Before the NMI, PPUCTRL ($2000) was $80 (NMI on, base nametable $2000, VRAM increment 1, sprite and \
background patterns at $0000) and PPUMASK ($2001) was $1E. The cartridge uses vertical mirroring. The game shows \
nametable $2000 with no scrolling."""

SMS_PLAIN = """Your routine is called once per frame from the VBlank (frame) interrupt handler of an NTSC Sega \
Master System game, with the display on (256x192 screen, mode 4, name table at VRAM $3800). The handler has already \
saved every register (main and shadow set) and read the VDP control port $BF, which acknowledged the interrupt and \
reset the control port's byte latch; it calls your routine with CALL and returns with EI and RETI right after. Do not \
change VDP registers."""

TASKS = {
    "6502": {
        "pal": "Copy the 32 bytes at $0300-$031F to the palette, PPU $3F00-$3F1F, in order.",
        "row": "Zero page $00 holds a row number, 0 to 29. Copy the 32 bytes at $0300-$031F to that row of "
               "nametable 0: PPU address $2000 + row*32, left to right.",
        "col": "Zero page $00 holds a column number, 0 to 31. Copy the 30 bytes at $0300-$031D down that column "
               "of nametable 0: byte i goes to PPU address $2000 + i*32 + column.",
        "big": "First do the OAM DMA from page $02 (the sprite table at $0200-$02FF). Then copy the 160 bytes at "
               "$0300-$039F to consecutive PPU addresses, starting at the address in zero page $00 (low byte) and "
               "$01 (high byte). The destination is always inside nametable 0 and the 160 bytes end before $2400.",
        "huge": "First do the OAM DMA from page $02 (the sprite table at $0200-$02FF). Then copy the 256 bytes at "
                "$0300-$03FF to consecutive PPU addresses, starting at the address in zero page $00 (low byte) and "
                "$01 (high byte). The destination is always inside nametable 0 and the 256 bytes end before $2400.",
    },
    "z80": {
        "pal": "Copy the 32 bytes at $C100-$C11F to color RAM entries 0-31, in order.",
        "row": "The byte at $C000 holds a row number, 0 to 23. Copy the 64 bytes at $C100-$C13F (32 name table "
               "entries of 2 bytes, low byte first) to that row of the name table: VRAM $3800 + row*64, left to right.",
        "col": "The byte at $C000 holds a column number, 0 to 31. Copy the 24 name table entries at $C100-$C12F "
               "(2 bytes each, low byte first) down that column: entry i goes to VRAM $3800 + i*64 + column*2.",
        "big": "$C000 and $C001 hold a VRAM address (low byte first), $0000 to $3500. Copy the 768 bytes at "
               "$C100-$C3FF to consecutive VRAM addresses starting there.",
        "huge": "$C000 and $C001 hold a VRAM address (low byte first), $0000 to $3400. Copy the 1024 bytes at "
                "$C100-$C4FF to consecutive VRAM addresses starting there.",
    },
}

CASES = {
    ("6502", "pal"): [0, 1, 2], ("6502", "row"): [0, 13, 29], ("6502", "col"): [0, 17, 31],
    ("6502", "big"): [0x2000, 0x2120, 0x2200], ("6502", "huge"): [0x2000, 0x2100, 0x2200],
    ("z80", "pal"): [0, 1, 2], ("z80", "row"): [0, 11, 23], ("z80", "col"): [0, 15, 31],
    ("z80", "big"): [0x0000, 0x1000, 0x2400], ("z80", "huge"): [0x0000, 0x1000, 0x2400],
}


def chr_bin():
    r = random.Random("chr")
    return bytes(r.randrange(256) for _ in range(4096))


def tiles_bin():
    r = random.Random("tiles")
    return bytes(r.randrange(256) for _ in range(8192))


def case_data(cpu, task, case):
    """args (16 bytes), buf, and for the NES the two OAM pages."""
    r = random.Random(f"{cpu}-{task}-{case}")
    args = bytearray(16)
    if cpu == "6502":
        if task == "pal":
            buf = [r.choice([v for v in range(0x40) if v != 0x0D]) for _ in range(32)]
        elif task == "row":
            args[0] = case; buf = [r.randrange(256) for _ in range(32)]
        elif task == "col":
            args[0] = case; buf = [r.randrange(256) for _ in range(30)]
        else:
            n = 160 if task == "big" else 256
            args[0], args[1] = case & 0xFF, case >> 8; buf = [r.randrange(256) for _ in range(n)]
        buf = bytes(buf) + bytes(r.randrange(256) for _ in range(256 - len(buf)))
        oam = bytes(v for _ in range(64) for v in (r.randrange(16, 200), r.randrange(256), r.randrange(4), r.randrange(248)))
        oam2 = bytes(v for _ in range(64) for v in (r.randrange(16, 200), r.randrange(256), r.randrange(4), r.randrange(248))) \
            if task in ("big", "huge") else oam
        return dict(args=bytes(args), buf=buf, oam=oam, oam2=oam2)
    if task == "pal":
        buf = [r.randrange(64) for _ in range(32)]
    elif task == "row":
        args[0] = case; buf = [v for _ in range(32) for v in (r.randrange(256), r.choice([0, 2, 4, 6, 8, 10]))]
    elif task == "col":
        args[0] = case; buf = [v for _ in range(24) for v in (r.randrange(256), r.choice([0, 2, 4, 6, 8, 10]))]
    else:
        n = 768 if task == "big" else 1024
        args[0], args[1] = case & 0xFF, case >> 8; buf = [r.randrange(256) for _ in range(n)]
    buf = bytes(buf) + bytes(r.randrange(256) for _ in range(1024 - len(buf)))
    return dict(args=bytes(args), buf=buf)


def include_file(cpu, task, case):
    d = case_data(cpu, task, case)

    def rows(label, data, kw):
        out = [f"{label}:"]
        for i in range(0, len(data), 16):
            out.append(f"  {kw} " + ",".join(f"${b:02X}" for b in data[i:i + 16]))
        return out
    if cpu == "6502":
        lines = rows("case_args", d["args"], ".byte") + rows("case_buf", d["buf"], ".byte") + \
            rows("case_oam", d["oam"], ".byte") + rows("case_oam2", d["oam2"], ".byte")
    else:
        lines = rows("case_args", d["args"], "DB") + rows("case_buf", d["buf"], "DB")
    return "\n".join(lines) + "\n"


def expected(cpu, task, case):
    """(offset into the dump, bytes) pairs the routine must leave there.
    NES dump: nametable RAM (2 KB, $2000 = offset 0 with vertical mirroring),
    palette (32 at $800), OAM (256 at $820). SMS dump: VRAM 16 KB, then CRAM."""
    d = case_data(cpu, task, case)
    buf = d["buf"]
    if cpu == "6502":
        if task == "pal":
            # $3F10/$3F14/$3F18/$3F1C mirror $3F00/$3F04/$3F08/$3F0C, so entries 0, 4, 8, 12 end up
            # with the later write; those four are checked against the reference run only
            return [(0x800 + i, buf[i:i + 1]) for i in range(32) if i % 4]
        if task == "row":
            return [(case * 32, buf[:32])]
        if task == "col":
            return [(i * 32 + case, buf[i:i + 1]) for i in range(30)]
        n = 160 if task == "big" else 256
        return [(case - 0x2000, buf[:n]), (0x820, d["oam2"])]
    if task == "pal":
        # the core keeps CRAM as one little-endian 16-bit word per entry
        return [(0x4000 + 2 * i, bytes([buf[i], 0])) for i in range(32)]
    if task == "row":
        return [(0x3800 + case * 64, buf[:64])]
    if task == "col":
        return [(0x3800 + i * 64 + case * 2, buf[2 * i:2 * i + 2]) for i in range(24)]
    return [(case, buf[:768 if task == "big" else 1024])]
