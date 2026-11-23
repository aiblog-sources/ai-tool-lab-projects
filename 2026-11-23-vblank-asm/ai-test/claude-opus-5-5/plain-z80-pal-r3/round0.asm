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
