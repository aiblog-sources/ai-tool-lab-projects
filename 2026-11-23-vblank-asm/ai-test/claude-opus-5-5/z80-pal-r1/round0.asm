; vblank_update - copy 32 bytes from $C100-$C11F to CRAM entries 0-31
; Called from the VBlank interrupt handler. The control port latch is already reset.
; VDP access speed doesn't matter during vblank, so OTIR is safe.

vblank_update:
        xor     a               ; 4   CRAM address low byte = $00
        out     ($BF),a         ; 11
        ld      a,$C0           ; 7   code 3 (CRAM write), address high = $00
        out     ($BF),a         ; 11
        ld      hl,$C100        ; 10  source
        ld      bc,$20BE        ; 10  B = 32 bytes, C = VDP data port
        otir                    ; 31*21 + 16 = 667
        ret                     ; 10
