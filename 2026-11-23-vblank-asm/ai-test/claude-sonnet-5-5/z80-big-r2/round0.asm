vblank_update:
        ld      hl,($C000)          ; VRAM destination address
        ld      a,l
        out     ($BF),a             ; low byte
        ld      a,h
        or      $40                 ; code 01 = VRAM write
        out     ($BF),a             ; high byte + command
        ld      hl,$C100            ; source in RAM
        ld      c,$BE               ; VDP data port
        ld      d,48                ; 48 * 16 = 768 bytes
.loop:
        REPT 16
        outi                        ; 16 cycles each (B is just a dummy counter)
        ENDR
        dec     d
        jp      nz,.loop
        ret
