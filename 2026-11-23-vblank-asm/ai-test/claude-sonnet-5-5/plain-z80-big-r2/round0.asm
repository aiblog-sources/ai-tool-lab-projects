vblank_update:
        ld      a,($C000)           ; VRAM address low byte
        out     ($BF),a             ; control port (latch already reset)
        ld      a,($C001)           ; VRAM address high byte
        or      $40                 ; command: VRAM write
        out     ($BF),a

        ld      hl,$C100            ; source in RAM
        ld      c,$BE               ; VDP data port
        ld      d,48                ; 48 * 16 = 768 bytes
.loop:
        REPT 16
        outi                        ; 16 cycles/byte, no VRAM timing issue in VBlank
        ENDR
        dec     d
        jr      nz,.loop
        ret
