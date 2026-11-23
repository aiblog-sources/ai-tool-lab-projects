The routine sets the VDP address to color RAM entry 0, then sends the 32 palette bytes to the data port with `OTIR`. VBlank has no VRAM/CRAM access-speed limit, so the 21-cycle `OTIR` writes are safe. The handler has already reset the control port's byte latch, so two control writes set up the address cleanly.

```asm
; vblank_update
; Copies 32 bytes from RAM $C100-$C11F into CRAM entries 0-31.
; Called from the VBlank interrupt handler (display on, inside VBlank).
; The handler already read $BF, so the control-port byte latch is reset.

vblank_update:
        ; Set the VDP address to CRAM $00 (code %11 = CRAM write).
        xor     a
        out     ($BF),a             ; address low byte = $00
        ld      a,$C0
        out     ($BF),a             ; code %11, address high bits = 0

        ; Stream 32 bytes to the VDP data port.
        ld      hl,$C100
        ld      bc,$20BE            ; B = 32 bytes, C = data port $BE
        otir
        ret
```