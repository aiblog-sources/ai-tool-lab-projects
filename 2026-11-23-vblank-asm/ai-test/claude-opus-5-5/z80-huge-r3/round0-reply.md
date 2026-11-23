**This can't be done in one frame as specified.** An NTSC SMS has 262 lines of 228 CPU cycles. Lines 192–261 are outside the active display, which gives 70 × 228 = 15960 cycles of vertical blank, before subtracting interrupt latency and the handler's register saves. The fastest per-byte VRAM write from arbitrary RAM data is `OUTI` at 16 cycles. `POP` plus `OUT (C),r` costs about 17 cycles per byte, and `LD A,(HL)` plus `OUT (n),A` costs more. So 1024 bytes need at least 1024 × 16 = 16384 cycles for the data writes alone, which is more than the whole vertical blank. Without turning the display off (a VDP register change) or writing during active display, both of which are forbidden, a full copy per call is impossible.

The routine below splits the copy across two calls instead:
- One scratch byte, `$C010`, toggles between calls.
- The first call writes `$C100`–`$C2FF` to VRAM starting at the address in `$C000`.
- The next call writes `$C300`–`$C4FF` to that address + 512.
- Each call does 512 unrolled `OUTI`s and fits easily in VBlank.
- The full 1024 bytes are in VRAM after two consecutive frames.
- If your harness checks the result after a single call, it will see only the first half. That is the unavoidable tradeoff.

```asm
; vblank_update - copy $C100-$C4FF to VRAM at ($C000), split over 2 frames
; Frame A (state 0 -> 1): $C100-$C2FF -> VRAM addr
; Frame B (state 1 -> 0): $C300-$C4FF -> VRAM addr + $200
vblank_update:
        ld      hl,$C010
        ld      a,(hl)
        xor     1
        and     1               ; normalise (RAM may start as garbage)
        ld      (hl),a          ; new state; NZ -> do first half now
        ld      hl,($C000)      ; VRAM destination
        ld      de,$C100
        jr      nz,.go
        ld      de,$C300        ; second half source
        ld      bc,$0200
        add     hl,bc           ; second half destination
.go:
        ld      a,l
        out     ($BF),a
        ld      a,h
        or      $40             ; VRAM write command
        out     ($BF),a
        ex      de,hl           ; HL = source
        ld      c,$BE           ; VDP data port
        rept    512
        outi
        endr
        ret
```

VBlank budget: 15960
Cycles used: 8356