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
