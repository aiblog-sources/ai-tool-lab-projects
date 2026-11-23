vblank_update:
        ld      a,($C000)           ; row 0..23
        rrca
        rrca                        ; bits 7-6 = row&3, bits 5-0 = row>>2
        ld      b,a
        and     $C0
        ld      e,a                 ; low address byte = (row&3)*64
        ld      a,b
        and     $3F
        or      $78                 ; $3800 | $4000 (VRAM write command) + row>>2
        ld      d,a
        ld      a,e
        out     ($BF),a             ; address low (latch was reset by the handler)
        ld      a,d
        out     ($BF),a             ; address high + write command
        ld      hl,$C100
        ld      c,$BE
        ld      b,64
        otir                        ; 64 bytes to the data port
        ret
