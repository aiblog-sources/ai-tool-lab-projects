; vblank_update: copy $0300-$031F into row ($00) of nametable 0.
; Called from NMI. A/X/Y are free to change.

vblank_update:
        bit $2002               ; 4  reset the PPU address/scroll latch

        ; High byte = $20 + (row >> 3)
        lda $00                 ; 3
        lsr a                   ; 2
        lsr a                   ; 2
        lsr a                   ; 2
        ora #$20                ; 2
        sta $2006               ; 4

        ; Low byte = (row << 5) & $FF
        lda $00                 ; 3
        asl a                   ; 2
        asl a                   ; 2
        asl a                   ; 2
        asl a                   ; 2
        asl a                   ; 2
        sta $2006               ; 4

        ; Copy 32 bytes, fully unrolled (8 cycles per byte)
        .repeat 32, I
        lda $0300+I             ; 4
        sta $2007               ; 4
        .endrepeat

        ; Restore scroll to 0,0 and PPUCTRL to $80 (nametable $2000).
        ; The latch is back on the first write after the two $2006 writes.
        lda #$00                ; 2
        sta $2005               ; 4  X scroll
        sta $2005               ; 4  Y scroll
        lda #$80                ; 2
        sta $2000               ; 4  NMI on, nametable $2000, increment 1
        rts
