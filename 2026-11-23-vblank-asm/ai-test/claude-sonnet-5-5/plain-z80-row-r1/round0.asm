vblank_update:
        ld      a,($C000)       ; row 0..23
        ld      b,a             ; keep a copy
        and     3
        rrca
        rrca                    ; (row & 3) << 6 = low byte of row*64
        out     ($BF),a         ; VRAM address low byte
        ld      a,b
        srl     a
        srl     a               ; row >> 2 = high byte of row*64
        add     a,$78           ; + $38 (name table base) + $40 (write command)
        out     ($BF),a         ; VRAM address high byte, set up for write
        ld      hl,$C100
        ld      b,64
        ld      c,$BE           ; VDP data port
        otir                    ; copy 64 bytes to the name table row
        ret
