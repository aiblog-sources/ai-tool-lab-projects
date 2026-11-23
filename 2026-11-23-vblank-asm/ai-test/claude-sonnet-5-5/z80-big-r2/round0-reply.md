```asm
vblank_update:
        ld      hl,($C000)          ; VRAM destination address
        ld      a,l
        out     ($BF),a             ; low byte
        ld      a,h
        or      $40                 ; code 01 = VRAM write
        out     ($BF),a             ; high byte + command
        ld      hl,$C100            ; source in RAM
        ld      c,$BE               ; VDP data port
        ld      d,48                ; 48 * 16 = 768 bytes
.loop:
        REPT 16
        outi                        ; 16 cycles each (B is just a dummy counter)
        ENDR
        dec     d
        jp      nz,.loop
        ret
```
VBlank budget: 15960
Cycles used: 13040

The budget is 70 vblank lines × 228 cycles per line on NTSC (262 lines total, 192 active). The count assumes 16 `OUTI`s per iteration, which gives 47 × 270 + 256 for the loop. The 94 cycles of setup (including the CALL) are added to that.

I have not assembled or run this.