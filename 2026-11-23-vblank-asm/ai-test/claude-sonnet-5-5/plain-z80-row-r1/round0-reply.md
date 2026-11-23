```asm
vblank_update:
        ld      a,($C000)       ; row 0..23
        ld      b,a             ; keep a copy
        and     3
        rrca
        rrca                    ; (row & 3) << 6 = low byte of row*64
        out     ($BF),a         ; VRAM address low byte
        ld      a,b
        srl     a
        srl     a               ; row >> 2 = high byte of row*64
        add     a,$78           ; + $38 (name table base) + $40 (write command)
        out     ($BF),a         ; VRAM address high byte, set up for write
        ld      hl,$C100
        ld      b,64
        ld      c,$BE           ; VDP data port
        otir                    ; copy 64 bytes to the name table row
        ret
```

The row address is `$3800 + row*64`. Its low byte is `(row & 3) << 6`, and its high byte is `$38 + (row >> 2)`. Adding `$40` to the high byte turns it into a VRAM write command, so the code adds `$78` in one step. The handler has already reset the control port's byte latch, so the two address bytes go out in the right order. The routine runs at the start of VBlank, so the 64-byte `OTIR` to the data port is safe.