```asm
vblank_update:
        ld      hl,(#C000)              ; 16  VRAM destination address
        ld      a,l                     ; 4
        out     ($BF),a                 ; 11  address low byte
        ld      a,h                     ; 4
        or      $40                     ; 7   command: VRAM write
        out     ($BF),a                 ; 11  address high byte
        ld      hl,$C100                ; 10  source
        ld      c,$BE                   ; 7   VDP data port
        REPT    960
        outi                            ; 16 each
        ENDR
        ret
```

I did not copy all 1024 bytes, because they can't fit in one NTSC vblank.

- The fastest Z80 way to feed the VDP is `OUTI` at 16 cycles per byte, so 1024 bytes need 16384 cycles. The whole vblank is only about 15960 cycles, and the interrupt handler's register saves have already used some of that.
- The routine therefore copies the first 960 bytes, `$C100-$C4BF`. The last 64 bytes, `$C4C0-$C4FF`, are not copied.
- I did not add a cross-frame progress counter, since the scratch RAM may not be initialised and the source data may change between frames. If you want the full 1024 bytes, copy 512 per frame and alternate halves. That needs a persistent flag in the scratch area, initialised by your startup code.

VBlank budget: 15960
Cycles used: 15447