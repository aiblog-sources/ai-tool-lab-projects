vblank_update:
        ; Set VRAM write address (the control port latch was already reset by the handler)
        ld      a,($C000)
        out     ($BF),a
        ld      a,($C001)
        or      $40             ; code 1 = VRAM write
        out     ($BF),a

        ld      hl,$C100
        ld      c,$BE           ; VDP data port
        ld      d,48            ; 48 blocks x 16 bytes = 768
.block:
        ld      b,16
        DUP 16
        outi                    ; 16 cycles/byte (OTIR at 21 would overrun vblank)
        EDUP
        dec     d
        jp      nz,.block
        ret
