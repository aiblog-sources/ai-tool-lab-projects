```asm
vblank_update:
        ld      hl,$C100        ; source: 32 bytes of palette data
        xor     a
        out     ($BF),a         ; CRAM address low byte = 0
        ld      a,$C0           ; code 3 (CRAM write), address bits high = 0
        out     ($BF),a         ; latch complete, CRAM write address = 0
        ld      c,$BE           ; VDP data port
        ld      b,32            ; 32 colour entries
        otir                    ; copy all 32 bytes to CRAM
        ret
```

VBlank budget: 15960
Cycles used: 741

The budget is 70 lines (lines 192-261) at 228 cycles each. It is not reduced for the interrupt handler's own overhead. The 741 cycles are CALL 17, setup and the two control-port writes 57, `ld c`/`ld b` 14, and OTIR 667 (31×21 + 16). The `ret` is not counted.