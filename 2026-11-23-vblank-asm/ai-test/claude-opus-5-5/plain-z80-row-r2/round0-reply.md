The routine computes the VRAM write address `$3800 + row*64` with the write-command bit (`$4000`) set. It sends that address to the control port, low byte first, then uses `OTIR` to send the 64 bytes to the data port.

```asm
; vblank_update
; Copies the 64 bytes at $C100-$C13F to name table row ($C000)
; at VRAM $3800 + row*64.
; Runs in VBlank, so there is no VRAM access-speed limit and OTIR is safe.
; The control port latch has already been reset by the handler's read of $BF.

vblank_update:
        ld      a,($C000)       ; row number, 0..23
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

        ld      hl,$C100        ; source: 32 entries x 2 bytes
        ld      bc,$40BE        ; B = 64 bytes, C = VDP data port
        otir
        ret
```