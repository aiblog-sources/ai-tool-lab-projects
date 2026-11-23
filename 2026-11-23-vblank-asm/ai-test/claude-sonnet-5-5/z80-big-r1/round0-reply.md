```asm
vblank_update:
        ld      hl,($C000)          ; VRAM destination address
        ld      c,$BF
        out     (c),l               ; address low byte
        ld      a,h
        or      $40                 ; $40 = VRAM write command
        out     (c),a               ; address high byte + command
        ld      hl,$C100            ; source
        ld      c,$BE               ; VDP data port
        ld      b,0                 ; OUTI counts B down: 256 bytes per pass
        ld      d,3                 ; 3 passes x 256 = 768 bytes
.pass:
.blk:
        REPT 16
        outi
        ENDR
        jp      nz,.blk             ; Z set once B has wrapped to 0 (256 bytes done)
        dec     d
        jp      nz,.pass
        ret
```

VBlank budget: 15960
Cycles used: 12892

The budget assumes 70 vblank lines of 228 cycles on NTSC, with no deduction for the handler's own overhead. The 12892 count runs from the CALL through the last OUTI write. The trailing JP, DEC and RET add 34 more cycles, so the whole routine takes 12926.