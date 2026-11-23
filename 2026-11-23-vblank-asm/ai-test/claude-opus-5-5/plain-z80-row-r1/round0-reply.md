The routine builds the VRAM write address for the row, sends it to the VDP control port, then streams the 64 bytes to the data port.

```asm
; vblank_update
; Copies 64 bytes from RAM $C100-$C13F to name table row ($C000)
; at VRAM $3800 + row*64.
; Called during VBlank, so OTIR (21 cycles per byte) is safe for VRAM writes.

vblank_update:
        ld      a,($C000)       ; row number, 0 to 23
        ld      l,a
        ld      h,0
        add     hl,hl           ; *2
        add     hl,hl           ; *4
        add     hl,hl           ; *8
        add     hl,hl           ; *16
        add     hl,hl           ; *32
        add     hl,hl           ; *64
        ld      de,$3800 | $4000 ; name table base + VRAM write command
        add     hl,de

        ld      a,l
        out     ($BF),a         ; address low byte
        ld      a,h
        out     ($BF),a         ; address high byte + write command

        ld      hl,$C100
        ld      bc,(64 << 8) | $BE ; B = 64 bytes, C = VDP data port
        otir
        ret
```