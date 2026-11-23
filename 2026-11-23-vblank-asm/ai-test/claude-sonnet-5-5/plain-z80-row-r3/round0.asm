vblank_update:
        ld      a,($C000)       ; row 0..23
        ld      b,a
        and     3
        rrca
        rrca                    ; (row & 3) << 6 = low address byte
        out     ($BF),a
        ld      a,b
        srl     a
        srl     a               ; row >> 2
        add     a,$38           ; high byte of $3800 + row*64
        or      $40             ; VRAM write command
        out     ($BF),a
        ld      hl,$C100
        ld      b,64
        ld      c,$BE
        otir                    ; copy 64 bytes to the VDP data port
        ret
