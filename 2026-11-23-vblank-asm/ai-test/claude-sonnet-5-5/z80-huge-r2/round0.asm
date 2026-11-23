vblank_update:
        ld      hl,(#C000)              ; 16  VRAM destination address
        ld      a,l                     ; 4
        out     ($BF),a                 ; 11  address low byte
        ld      a,h                     ; 4
        or      $40                     ; 7   command: VRAM write
        out     ($BF),a                 ; 11  address high byte
        ld      hl,$C100                ; 10  source
        ld      c,$BE                   ; 7   VDP data port
        REPT    960
        outi                            ; 16 each
        ENDR
        ret
