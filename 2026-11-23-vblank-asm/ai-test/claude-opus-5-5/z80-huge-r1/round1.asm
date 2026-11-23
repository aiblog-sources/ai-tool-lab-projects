; vblank_update: copy the 1024 bytes $C100-$C4FF to VRAM starting at ($C000),
; 512 bytes per call, alternating halves on successive frames.
; Uses scratch byte $C010 as the phase toggle (bit 0).
vblank_update:
        ld      hl,$C010            ; 10
        ld      a,(hl)              ;  7
        xor     1                   ;  7  flip phase
        ld      (hl),a              ;  7
        ld      de,($C000)          ; 20  VRAM base address
        ld      hl,$C100            ; 10  first-half source
        rrca                        ;  4  phase bit -> carry
        jr      nc,.go              ; 12/7
        inc     d                   ;  4  VRAM address + $200
        inc     d                   ;  4
        ld      hl,$C300            ; 10  second-half source
.go:
        ld      a,e                 ;  4
        out     ($BF),a             ; 11  address low byte
        ld      a,d                 ;  4
        or      $40                 ;  7  VRAM write command
        out     ($BF),a             ; 11
        ld      c,$BE               ;  7  VDP data port
        REPT 512
        outi                        ; 16 each
        ENDR
        ret
