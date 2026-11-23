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
