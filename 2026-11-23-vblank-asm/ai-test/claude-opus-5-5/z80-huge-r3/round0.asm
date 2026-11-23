; vblank_update - copy $C100-$C4FF to VRAM at ($C000), split over 2 frames
; Frame A (state 0 -> 1): $C100-$C2FF -> VRAM addr
; Frame B (state 1 -> 0): $C300-$C4FF -> VRAM addr + $200
vblank_update:
        ld      hl,$C010
        ld      a,(hl)
        xor     1
        and     1               ; normalise (RAM may start as garbage)
        ld      (hl),a          ; new state; NZ -> do first half now
        ld      hl,($C000)      ; VRAM destination
        ld      de,$C100
        jr      nz,.go
        ld      de,$C300        ; second half source
        ld      bc,$0200
        add     hl,bc           ; second half destination
.go:
        ld      a,l
        out     ($BF),a
        ld      a,h
        or      $40             ; VRAM write command
        out     ($BF),a
        ex      de,hl           ; HL = source
        ld      c,$BE           ; VDP data port
        rept    512
        outi
        endr
        ret
