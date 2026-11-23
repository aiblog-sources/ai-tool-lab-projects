The full 1024 bytes can't fit in the vertical blank. `OUTI` at 16 cycles per byte is the fastest VDP write, so 1024 bytes would need 16384 cycles against 15960 available. The best I can do is copy the largest prefix that fits and leave the tail uncopied.

Last time the loop overhead (`dec d` / `jp nz`, 14 cycles per block) pushed the last write to 16048, past the end of blank. From that run, the CALL happens about 387 cycles after vertical blank starts. That leaves about 15573 cycles for my code. The setup costs 94 cycles, so 966 bytes fully unrolled end at about 15937 cycles after the start of blank, which is 23 cycles of margin. The last 58 bytes are not copied.

```asm
; vblank_update: copy the first 966 bytes of the buffer at $C100 to VRAM
; starting at the address held in ($C000). The last 58 bytes are NOT copied:
; 1024 * 16 cycles (OUTI) = 16384 > 15960-cycle NTSC VBlank, and the CALL
; itself happens ~387 cycles into VBlank. Fully unrolled to avoid loop overhead.

vblank_update:
    ld hl,($C000)           ; VRAM destination address
    ld a,l
    out ($BF),a             ; address low byte
    ld a,h
    or $40                  ; VRAM write command
    out ($BF),a             ; address high byte + command
    ld hl,$C100             ; source
    ld c,$BE                ; VDP data port
    REPT 966
    outi
    ENDR
    ret
```

VBlank budget: 15960
Cycles used: 15550