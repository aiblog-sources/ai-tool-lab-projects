vblank_update:
        xor     a
        out     ($BF),a         ; CRAM address low byte = 0
        ld      a,$C0
        out     ($BF),a         ; code 3 (CRAM write), address high bits = 0
        ld      hl,$C100        ; source buffer
        ld      bc,$20BE        ; B = 32 bytes, C = VDP data port $BE
        otir                    ; copy 32 bytes to CRAM entries 0-31
        ret
