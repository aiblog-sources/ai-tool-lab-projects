```asm
; vblank_update: copy 768 bytes from $C100-$C3FF to VRAM starting at the
; address stored at $C000 (low) / $C001 (high).
; The display is on, so the copy must finish inside vertical blank.
; A fully unrolled OUTI chain is used. At 16 cycles per byte it is the
; fastest way to stream bytes to the VDP, and the VDP accepts that rate
; during vblank.

vblank_update:
        ld      hl,($C000)          ; 16  VRAM destination address
        ld      a,l                 ;  4
        out     ($BF),a             ; 11  address low byte
        ld      a,h                 ;  4
        or      $40                 ;  7  code 01 = VRAM write
        out     ($BF),a             ; 11  address high byte + command
        ld      hl,$C100            ; 10  source buffer
        ld      c,$BE               ;  7  VDP data port
        rept    768
        outi                        ; 16 each, 768 x 16 = 12288
        endr
        ret
```

VBlank budget: 15960
Cycles used: 12375

The budget is 70 lines (262 - 192) × 228 cycles per line, which gives 15960. If the frame interrupt is taken one line late, at line 193, the budget is 15732. The handler also spends a few hundred cycles saving registers before the CALL. Either way, 12375 cycles leaves about 3000 cycles to spare.

The cycle count is CALL 17 + setup 70 + 768 × OUTI 16 = 12375.