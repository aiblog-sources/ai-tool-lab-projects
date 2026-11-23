```asm
; vblank_update
; Copies the 32 bytes at $C100-$C11F to CRAM entries 0-31.
; Called from the VBlank handler, so the VDP is in vertical blank.
; At this point OTIR's 21-cycle spacing between writes is safe.
; The control port latch was already reset when the handler read $BF.

vblank_update:
        xor     a
        out     ($BF),a         ; address low byte = $00
        ld      a,$C0
        out     ($BF),a         ; code 3 (CRAM write), address $00
        ld      hl,$C100        ; source buffer
        ld      bc,$20BE        ; B = 32 bytes, C = VDP data port
        otir                    ; write all 32 palette entries
        ret
```