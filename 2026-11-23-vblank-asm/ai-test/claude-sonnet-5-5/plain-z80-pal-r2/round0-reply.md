```asm
vblank_update:
        xor     a               ; CRAM address low byte = 0
        out     ($BF),a
        ld      a,$C0           ; code 3 (CRAM write), address high bits = 0
        out     ($BF),a
        ld      hl,$C100        ; source in RAM
        ld      b,32            ; 32 color entries
        ld      c,$BE           ; VDP data port
        otir                    ; copy 32 bytes to CRAM 0-31 (CRAM address auto-increments)
        ret
```

The handler has already reset the control port latch, so the first `out` is taken as the low address byte. Writing the second byte, `$C0`, sets the CRAM write command. The routine runs at the start of VBlank, so the 21-cycle-per-byte `OTIR` doesn't cause any display artifacts. It changes no VDP registers and writes no RAM.