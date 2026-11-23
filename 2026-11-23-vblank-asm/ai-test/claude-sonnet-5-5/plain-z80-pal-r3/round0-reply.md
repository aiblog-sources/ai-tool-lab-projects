```asm
vblank_update:
        xor     a
        out     ($BF),a         ; CRAM address low byte = 0
        ld      a,$C0
        out     ($BF),a         ; code 3 (CRAM write), address high bits = 0
        ld      hl,$C100        ; source buffer
        ld      bc,$20BE        ; B = 32 bytes, C = VDP data port $BE
        otir                    ; copy 32 bytes to CRAM entries 0-31
        ret
```

The handler's read of $BF already reset the control port latch, so the first control write is taken as the low address byte. The routine runs during VBlank, so the CRAM writes cause no display artifacts and need no extra timing delays. It does not change any VDP registers and writes no RAM.