vblank_update:
        ld   hl,($C000)         ; VRAM destination address
        ld   a,l
        out  ($BF),a            ; address low byte
        ld   a,h
        or   $40                ; write-to-VRAM command
        out  ($BF),a            ; address high byte + command
        ld   hl,$C100           ; source
        ld   c,$BE              ; VDP data port
        rept 966
        outi                    ; 16 cycles per byte, fully unrolled
        endr
        ret
