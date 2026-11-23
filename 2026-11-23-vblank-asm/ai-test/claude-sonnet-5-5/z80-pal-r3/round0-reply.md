```asm
vblank_update:
        ld   hl,$C100           ; source buffer
        xor  a                  ; CRAM address low byte = 0
        out  ($BF),a
        ld   a,$C0              ; code 3 (CRAM write), address high bits = 0
        out  ($BF),a
        ld   c,$BE              ; VDP data port
        ld   b,32               ; 32 bytes = CRAM entries 0-31
        otir
        ret
```

The handler already reset the control port latch, so the first `out` is the low address byte and the second is the high byte plus command. `otir` runs at 21 cycles per byte, which is slow enough for the VDP.

VBlank budget: 15960
Cycles used: 741