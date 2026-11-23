The old routine cannot fit in one frame, so this version spreads the copy over two frames: 512 bytes per frame, alternating halves.

**Why 1024 bytes won't fit:** From your measurements, the last write ended at 16871 cycles. My code from the CALL to the last write takes 17 + 70 + 1024×16 = 16471 cycles. That leaves about 400 cycles of vertical blank used before the CALL, so about 15560 remain for the routine. That is about 15.2 cycles per byte. `OUTI` at 16 cycles per byte is the fastest way the Z80 can send RAM to the VDP. Every other method costs 17 or more per byte, because each byte must also be loaded into a register. No VDP register changes are allowed, so there is no faster path.

**What this means for you:** VRAM only holds the complete 1024 bytes after two consecutive frames. If the buffer at $C100 changes between those two frames, VRAM briefly holds a mix of old and new data. The halves are chosen by a toggle bit in scratch RAM at $C010. It works from any starting value, but which half goes first is not fixed.

```asm
; vblank_update: copy the 1024 bytes $C100-$C4FF to VRAM starting at ($C000),
; 512 bytes per call, alternating halves on successive frames.
; Uses scratch byte $C010 as the phase toggle (bit 0).
vblank_update:
        ld      hl,$C010            ; 10
        ld      a,(hl)              ;  7
        xor     1                   ;  7  flip phase
        ld      (hl),a              ;  7
        ld      de,($C000)          ; 20  VRAM base address
        ld      hl,$C100            ; 10  first-half source
        rrca                        ;  4  phase bit -> carry
        jr      nc,.go              ; 12/7
        inc     d                   ;  4  VRAM address + $200
        inc     d                   ;  4
        ld      hl,$C300            ; 10  second-half source
.go:
        ld      a,e                 ;  4
        out     ($BF),a             ; 11  address low byte
        ld      a,d                 ;  4
        or      $40                 ;  7  VRAM write command
        out     ($BF),a             ; 11
        ld      c,$BE               ;  7  VDP data port
        REPT 512
        outi                        ; 16 each
        ENDR
        ret
```

VBlank budget: 15960
Cycles used: 8343