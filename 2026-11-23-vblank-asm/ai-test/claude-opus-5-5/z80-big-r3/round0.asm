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
