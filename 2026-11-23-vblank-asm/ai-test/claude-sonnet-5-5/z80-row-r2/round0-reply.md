```asm
vblank_update:
        ld   a,($C000)          ; row 0..23
        rrca
        rrca                    ; bits 7-6 = (row&3), bits 5-0 = row>>2
        ld   b,a
        and  $C0                ; low byte of VRAM address = (row&3)<<6
        out  ($BF),a
        ld   a,b
        and  $3F
        or   $78                ; $38 (name table base) | $40 (VRAM write command)
        out  ($BF),a
        ld   hl,$C100
        ld   c,$BE              ; VDP data port
        ld   b,64
        otir                    ; 64 bytes, low byte of each entry first
        ret
```

VBlank budget: 15960
Cycles used: 1452

The budget assumes about 70 vblank lines (lines 192-261 of 262) at 228 cycles each. The cycle count covers CALL through the last OTIR write and excludes the final RET.