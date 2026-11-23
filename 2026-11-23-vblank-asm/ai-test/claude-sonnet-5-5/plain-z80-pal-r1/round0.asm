vblank_update:
        xor a                   ; CRAM address 0
        out ($BF),a             ; low byte of address (latch already reset)
        ld a,$C0                ; code 3 = CRAM write, address high bits 0
        out ($BF),a
        ld hl,$C100             ; source buffer
        ld c,$BE                ; VDP data port
        ld b,32                 ; 32 colour entries
        otir                    ; copy 32 bytes to CRAM 0-31
        ret
