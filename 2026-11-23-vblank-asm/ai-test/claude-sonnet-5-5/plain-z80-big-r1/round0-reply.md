```asm
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
```

The copy uses unrolled `OUTI` rather than `OTIR`. `OTIR` costs 21 cycles per byte, so 768 bytes would take about 16,100 cycles. That is more than the roughly 15,960 cycles of NTSC vblank, so the writes would run into the active display. `OUTI` takes 16 cycles per byte, so the whole copy takes about 12,900 cycles and fits inside vblank.

The VRAM range cannot wrap: the largest start address is $3500, and $3500 + $300 = $3800.