vblank_update:
        ld   hl,$C100           ; source buffer
        xor  a                  ; CRAM address low byte = 0
        out  ($BF),a
        ld   a,$C0              ; code 3 (CRAM write), address high bits = 0
        out  ($BF),a
        ld   c,$BE              ; VDP data port
        ld   b,32               ; 32 bytes = CRAM entries 0-31
        otir
        ret
