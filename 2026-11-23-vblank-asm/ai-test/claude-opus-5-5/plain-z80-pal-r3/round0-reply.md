The routine points the VDP at color RAM address 0, then streams the 32 bytes to the data port with OTIR.

- **Setting the address:** writing `$00` and then `$C0` to the control port `$BF` sets the address to 0 and selects color RAM as the write target. The handler has already reset the port's byte latch, so the two bytes are taken in the right order.
- **Copying:** OTIR sends one byte about every 21 cycles. That is safe here because during VBlank the VDP accepts data at any speed.
- **Limits:** it doesn't change any VDP registers or touch any RAM outside the source buffer.

```asm
; vblank_update
; Copies 32 bytes from RAM $C100-$C11F into CRAM entries 0-31.
; Called from the VBlank interrupt handler (control port latch already reset).
vblank_update:
        xor     a
        out     ($BF), a        ; address low byte = $00
        ld      a, $C0
        out     ($BF), a        ; code %11 = CRAM write, address high = 0
        ld      hl, $C100       ; source buffer
        ld      bc, $20BE       ; B = 32 bytes, C = VDP data port
        otir                    ; 21 cycles/byte, safe during VBlank
        ret
```