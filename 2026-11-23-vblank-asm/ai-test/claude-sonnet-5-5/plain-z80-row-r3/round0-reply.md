```asm
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
```

The VDP address is `$3800 + row*64`. The low byte is `(row & 3) << 6`, and the high byte is `$38 + (row >> 2)`, with bit 6 set (`$40`) to select a VRAM write. The control port latch was already reset by the handler, so the low byte goes out first. `OTIR` then sends all 64 bytes, which fits easily within VBlank.