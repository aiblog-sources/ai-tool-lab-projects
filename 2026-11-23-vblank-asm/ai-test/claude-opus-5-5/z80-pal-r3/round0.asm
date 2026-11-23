; vblank_update
; Copies 32 bytes from RAM $C100-$C11F into CRAM entries 0-31.
; Called from the VBlank handler, so the display is in vertical blank.
; The handler has already read $BF, so the control-port byte latch is reset.

vblank_update:
        ; Set the VDP address to CRAM entry 0.
        ; The control word is $C000: low byte $00 first, then $C0
        ; (code 3 means a CRAM write).
        xor     a                   ; 4   A = $00
        out     ($BF),a             ; 11  address low byte
        ld      a,$C0               ; 7
        out     ($BF),a             ; 11  code 3 (CRAM write), address high = 0

        ; Stream the 32 palette bytes to the VDP data port.
        ld      hl,$C100            ; 10  source address
        ld      bc,$20BE            ; 10  B = 32 bytes, C = data port $BE
        otir                        ; 31*21 + 16 = 667
                                    ; writes come 21 cycles apart, which is fine in VBlank
        ret                         ; 10
