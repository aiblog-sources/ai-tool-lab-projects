; vblank_update
; Copies 64 bytes from $C100-$C13F to name table row ($C000) at VRAM $3800 + row*64.
; Called from the VBlank interrupt handler (display on, in VBlank, latch already reset).

vblank_update:
        ld      a,($C000)       ; row number 0..23
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
        ld      a,l
        out     ($BF),a         ; address low byte
        ld      a,h
        out     ($BF),a         ; address high byte + write command
        ld      hl,$C100        ; source buffer
        ld      bc,$40BE        ; B = 64 bytes, C = VDP data port
        otir                    ; 21 cycles/byte, safe during VBlank
        ret
