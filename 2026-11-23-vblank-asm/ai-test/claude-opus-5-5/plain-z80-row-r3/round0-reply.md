The routine computes the VRAM address `$3800 + row*64`, sets the write flag (`$4000`), and sends that address to the VDP control port. It then streams the 64 bytes to the data port with `OTIR`.

`OTIR` takes 21 cycles per byte, and that is safe during VBlank. The VDP's byte latch was already reset by the handler's read of `$BF`, so the address can be written immediately.

```asm
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
```