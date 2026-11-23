vblank_update:
        ld      hl,$C100        ; source: 32 bytes of palette data
        xor     a
        out     ($BF),a         ; CRAM address low byte = 0
        ld      a,$C0           ; code 3 (CRAM write), address bits high = 0
        out     ($BF),a         ; latch complete, CRAM write address = 0
        ld      c,$BE           ; VDP data port
        ld      b,32            ; 32 colour entries
        otir                    ; copy all 32 bytes to CRAM
        ret
