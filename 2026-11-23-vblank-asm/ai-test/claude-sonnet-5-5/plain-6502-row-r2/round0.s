vblank_update:
        lda $2002               ; reset PPU address latch
        lda $00                 ; row 0..29
        lsr a
        lsr a
        lsr a                   ; row >> 3
        ora #$20                ; high byte = $20 + (row >> 3)
        sta $2006
        lda $00
        and #$07
        asl a
        asl a
        asl a
        asl a
        asl a                   ; low byte = (row & 7) << 5
        sta $2006

        ldx #0
@copy:  lda $0300,x
        sta $2007
        inx
        cpx #32
        bne @copy

        ; Restore scroll: the $2006 writes clobbered the t register,
        ; including its nametable bits.
        lda $2002               ; reset latch
        lda #$80
        sta $2000               ; t nametable bits = 0 (NMI stays on, increment 1)
        lda #0
        sta $2005               ; X scroll = 0
        sta $2005               ; Y scroll = 0
        rts
