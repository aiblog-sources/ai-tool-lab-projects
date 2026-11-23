The task cannot be done as specified. I've given the fastest correct copy below, but it runs about 500 cycles past the end of vertical blank.

**Why it doesn't fit:**
- An NTSC Master System has 262 lines of 228 Z80 cycles each. 192 of those lines are active display, leaving 70 lines of vertical blank. That is 70 × 228 = **15,960 cycles**, counted from the very first blank line, before interrupt latency and the handler's register saves take their share.
- The cheapest way to send an arbitrary RAM byte to the VDP data port is `OUTI`, at 16 cycles per byte. The alternatives are slower:
  - `POP rr` followed by two `OUT (C),r` costs 34 cycles per 2 bytes, so 17 per byte.
  - `LD A,(HL)` / `INC L` / `OUT (n),A` costs 22 per byte.
  - `POP AF` puts only one useful byte in A.
- So 1024 bytes need at least 1024 × 16 = **16,384 cycles** for the data writes alone, which is more than the whole vertical blank.
- The two usual fixes are both ruled out by your rules. You can't blank the display, because VDP registers are off-limits. You can't trickle writes into active display, because all writes must finish inside vertical blank.

```asm
; vblank_update: copy 1024 bytes $C100-$C4FF to VRAM at address in ($C000).
; Fastest possible form (fully unrolled OUTI, 16 cycles/byte), but
; 1024 bytes cannot fit in NTSC vblank: see notes below the code.
vblank_update:
        ld      hl,($C000)          ; 16  VRAM destination address
        ld      a,l                 ;  4
        out     ($BF),a             ; 11  address low byte
        ld      a,h                 ;  4
        or      $40                 ;  7  VRAM write command
        out     ($BF),a             ; 11  address high byte + command
        ld      hl,$C100            ; 10  source
        ld      c,$BE               ;  7  VDP data port
        REPT 1024
        outi                        ; 16 each
        ENDR
        ret
```

VBlank budget: 15960
Cycles used: 16471

That is 17 (CALL) + 69 (setup) + 16,384 (data). This is 511 cycles over the full blank period and more once the handler overhead is included. So the last roughly 35 or more bytes land during active display, where they may be dropped or corrupt the picture.

**Ways to make it fit, each of which needs a rule changed:**
1. **Split across two frames:** copy 512 bytes per frame, keeping the progress in $C010. That takes about 8,300 cycles per frame and fits easily.
2. **Shrink the transfer:** at 16 cycles per byte, about 950 bytes is the practical maximum after interrupt and handler overhead.
3. **Blank the display:** clearing the display-enable bit in VDP register 1 allows unrestricted writes, but this breaks the "don't change VDP registers" rule and makes the screen flicker.

I can write version 1 if the 2-frame latency is acceptable.