vblank_update:
        ld      hl,$C100            ; source: 32 bytes of palette data
        xor     a                   ; CRAM address 0 (low byte)
        out     ($BF),a
        ld      a,$C0               ; code 3 = CRAM write (high byte)
        out     ($BF),a
        ld      b,32                ; 32 bytes
        ld      c,$BE               ; VDP data port
        otir                        ; copy to CRAM entries 0-31
        ret
