This task cannot be done within the stated limits. 1024 bytes do not fit in an NTSC vertical blank. The fastest way to send a byte to the VDP data port is `OUTI` at 16 cycles per byte. 1024 bytes therefore need at least 16,384 cycles for the data alone. NTSC vertical blank is 262 − 192 = 70 lines × 228 cycles = 15,960 cycles. The interrupt arrives about a line late, and your handler's saves and its port $BF read use more of that time, so the real window is smaller still.

Other approaches are slower, not faster:
- `POP` followed by `OUT (C),r` costs about 17 cycles per byte.
- `LD A,r` followed by `OUT (n),A`, including reloading the registers, costs about 20 cycles per byte.

Below is the fastest correct routine: a minimal VRAM address setup followed by 1024 unrolled `OUTI`s. It still overruns the window by about 500–800 cycles. That means roughly the last 30–50 bytes would be written after the active display starts, where the VDP may drop them.

```asm
; vblank_update - copy 1024 bytes $C100-$C4FF to VRAM at address in ($C000)
; Fastest possible form: 16 cycles/byte via unrolled OUTI.
; NOTE: exceeds the NTSC vblank window (see notes below the code).
vblank_update:
        ld      hl,($C000)      ; 16  VRAM destination address
        ld      a,l             ;  4
        out     ($BF),a         ; 11  address low byte
        ld      a,h             ;  4
        or      $40             ;  7  VRAM write command
        out     ($BF),a         ; 11  address high byte + command
        ld      hl,$C100        ; 10  source
        ld      c,$BE           ;  7  VDP data port
        rept 1024
        outi                    ; 16 each
        endr
        ret
```

VBlank budget: 15960
Cycles used: 16471

That is 17 for the CALL, plus 70 for setup, plus 16,384 for the `OUTI`s. This is more than the budget, so the routine does not meet the "finish inside vertical blank" rule. No Z80 code can meet that rule for 1024 bytes on NTSC.

To get a version that fully follows the rules, one of the constraints has to change. Any one of these would work:
- **Split the copy over two frames.** Copy 512 bytes per vblank and keep a progress flag in $C010. Each half takes about 8.3k cycles, which fits easily. The catch is that VRAM is only complete every second frame.
- **Blank the display during the copy.** Turn the display off with VDP register 1 for the copy, which removes the time limit. That breaks the "do not change VDP registers" rule.
- **Copy less data.** At most about 940–960 bytes fit, depending on how long your handler's preamble takes.

Tell me which option you prefer and I'll write that version.