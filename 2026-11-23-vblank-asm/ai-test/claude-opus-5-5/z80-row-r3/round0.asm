; vblank_update
; Copies 64 bytes from $C100-$C13F to name table row ($C000) at VRAM $3800 + row*64.
; The control port latch is already reset by the handler's read of $BF.
; Everything runs inside VBlank, so OTIR's 21-cycle write spacing is safe.

vblank_update:
        ld      a,($C000)       ; row 0..23
        ld      l,a
        ld      h,0
        add     hl,hl           ; *2
        add     hl,hl           ; *4
        add     hl,hl           ; *8
        add     hl,hl           ; *16
        add     hl,hl           ; *32
        add     hl,hl           ; *64
        ld      de,$7800        ; $3800 | $4000 (VRAM write command)
        add     hl,de
        ld      c,$BF
        out     (c),l           ; address low byte
        out     (c),h           ; address high byte + write command
        ld      hl,$C100
        ld      bc,$40BE        ; B = 64 bytes, C = VDP data port
        otir
        ret
