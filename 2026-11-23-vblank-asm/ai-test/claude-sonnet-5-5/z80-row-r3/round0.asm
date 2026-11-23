vblank_update:
        ld   a,($C000)          ; row 0..23
        rrca
        rrca                    ; bits 1..0 -> bits 7..6, row>>2 -> bits 5..0
        ld   c,a
        and  $C0                ; low byte of row*64
        out  ($BF),a            ; VRAM address low
        ld   a,c
        and  $3F                ; row>>2
        add  a,$78              ; $3800 | $4000 (write command)
        out  ($BF),a            ; VRAM address high + write command
        ld   hl,$C100
        ld   c,$BE              ; VDP data port
        ld   b,64
        otir                    ; 64 bytes: name table entries, low byte first
        ret
