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
