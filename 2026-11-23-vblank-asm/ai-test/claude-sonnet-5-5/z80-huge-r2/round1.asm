vblank_update:
        ld      a,($C010)               ; flip a phase bit kept in scratch RAM
        xor     1
        ld      ($C010),a
        ld      hl,($C000)              ; VRAM destination
        ld      de,$C100                ; source
        rrca                            ; carry = phase bit
        jr      nc,.go
        ld      bc,64                   ; odd frames: copy bytes 64..1023
        add     hl,bc
        ld      de,$C140
.go:
        ld      a,l
        out     ($BF),a                 ; address low
        ld      a,h
        or      $40                     ; VRAM write command
        out     ($BF),a                 ; address high
        ex      de,hl                   ; HL = source
        ld      c,$BE                   ; VDP data port
        REPT    960
        outi
        ENDR
        ret
