A 1024-byte copy cannot fit in one NTSC vertical blank. The fastest write is unrolled `OUTI` at 16 cycles per byte, so 1024 bytes take at least 16,384 cycles. The whole vertical blank is only 70 lines × 228 = 15,960 cycles, and your routine starts about 430 cycles into it.

So this version spreads the copy over two frames, 512 bytes each. It increments a counter byte in scratch RAM ($C010) on every call, and bit 0 of that counter picks which half to copy:
- **Even call:** $C100-$C2FF goes to the VRAM address.
- **Odd call:** $C300-$C4FF goes to the VRAM address + $200.

Because only the counter's parity matters, its starting value doesn't matter. After any two consecutive frames the full 1024 bytes are in VRAM, as long as the buffer and the address don't change in between. If the harness checks for the complete copy after a single frame, no correct routine can meet that.

```asm
; vblank_update - copy 1024 bytes $C100-$C4FF to VRAM at address in ($C000)
; 1024 bytes at the fastest rate (OUTI, 16 cycles/byte) = 16384 cycles,
; more than a whole NTSC vblank (70 lines * 228 = 15960 cycles), so the
; copy is split over two frames: 512 bytes per call, alternating halves.
; A counter byte at $C010 (scratch) selects the half via bit 0; its
; initial value does not matter.
vblank_update:
        ld      hl,$C010        ; 10
        inc     (hl)            ; 11  frame toggle
        ld      a,(hl)          ;  7
        ld      hl,($C000)      ; 16  VRAM destination address
        ld      de,$C100        ; 10  source, first half
        rra                     ;  4  bit 0 -> carry
        jr      nc,.setaddr     ; 12/7
        inc     h               ;  4  second half: VRAM address + $200
        inc     h               ;  4
        ld      d,$C3           ;  7  source $C300
.setaddr:
        ld      a,l             ;  4
        out     ($BF),a         ; 11  address low byte
        ld      a,h             ;  4
        or      $40             ;  7  VRAM write command
        out     ($BF),a         ; 11  address high byte + command
        ex      de,hl           ;  4  HL = source
        ld      c,$BE           ;  7  VDP data port
        rept 512
        outi                    ; 16 each
        endr
        ret
```

VBlank budget: 15960
Cycles used: 8337