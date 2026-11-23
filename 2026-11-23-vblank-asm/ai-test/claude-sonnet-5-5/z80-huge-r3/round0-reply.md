```asm
vblank_update:
        ld   hl,($C000)         ; VRAM destination address
        ld   a,l
        out  ($BF),a            ; address low byte
        ld   a,h
        or   $40                ; write-to-VRAM command
        out  ($BF),a            ; address high byte + command
        ld   hl,$C100           ; source
        ld   c,$BE              ; VDP data port
        ld   d,15               ; 15 blocks of 64 bytes = 960 bytes
.block:
        rept 64
        outi                    ; 16 cycles per byte
        endr
        dec  d
        jp   nz,.block
        ret
```

**This routine copies only the first 960 of the 1024 bytes.** The full copy can't fit in one NTSC vblank under your rules:

- `OUTI` is the cheapest way to send a byte from RAM to the VDP. Every alternative I checked (`POP` plus `OUT (C),r`, `LD A,(HL)` plus `OUT (n),A`) costs 17 cycles per byte or more.
- 1024 bytes at 16 cycles is 16384 cycles, which is more than the 70 lines × 228 = 15960 cycles of NTSC vblank.
- The handler's register saves and interrupt latency also come out of that window.
- Display on and no VDP register changes rule out blanking the screen to get extra time.
- `$C000`/`$C001` can't be updated, so I can't carry the remaining 64 bytes over to the next frame.

I chose 960 bytes (60 lines of copying) to leave about 300 cycles of margin for the handler overhead. If you want all 1024 bytes, the copy has to be split across frames. The caller would keep a progress counter and advance the destination address, or the harness would need to allow display-off or a longer budget.

VBlank budget: 15960
Cycles used: 15650