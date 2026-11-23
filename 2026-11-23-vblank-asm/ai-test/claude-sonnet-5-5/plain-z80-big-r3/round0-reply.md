```asm
vblank_update:
        ld      a,($C000)           ; VRAM address low byte
        out     ($BF),a
        ld      a,($C001)           ; VRAM address high byte
        or      $40                 ; code 01 = VRAM write
        out     ($BF),a

        ld      hl,$C100            ; source
        ld      c,$BE               ; VDP data port
        ld      d,3                 ; 3 x 256 = 768 bytes
.pass:
        ld      b,0                 ; 256 bytes per pass
.loop:
        REPT 16
        outi                        ; 16 cycles/byte, B decrements
        ENDR
        jp      nz,.loop            ; B reaches 0 after 16 blocks
        dec     d
        jr      nz,.pass
        ret
```

The copy uses unrolled `OUTI` at 16 cycles per byte instead of `OTIR` at 21. That keeps the 768 bytes to about 12.8k cycles, which fits in the NTSC VBlank of roughly 16k cycles. The loop runs every pass on B, which `OUTI` decrements itself. Each pass is 16 blocks of 16 `OUTI`s, and `JP NZ` exits when B reaches 0. No VDP registers are touched. The VRAM address is written to port $BF with the latch already reset by the handler.