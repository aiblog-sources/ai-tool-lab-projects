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
