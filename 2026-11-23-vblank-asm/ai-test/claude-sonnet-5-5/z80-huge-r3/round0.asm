vblank_update:
        ld   hl,($C000)         ; VRAM destination address
        ld   a,l
        out  ($BF),a            ; address low byte
        ld   a,h
        or   $40                ; write-to-VRAM command
        out  ($BF),a            ; address high byte + command
        ld   hl,$C100           ; source
        ld   c,$BE              ; VDP data port
        ld   d,15               ; 15 blocks of 64 bytes = 960 bytes
.block:
        rept 64
        outi                    ; 16 cycles per byte
        endr
        dec  d
        jp   nz,.block
        ret
