vblank_update:
        lda $2002               ; reset the PPU address latch
        lda $00                 ; row number
        lsr a
        lsr a
        lsr a                   ; row >> 3
        ora #$20                ; high byte = $20 + (row >> 3)
        sta $2006
        lda $00
        asl a
        asl a
        asl a
        asl a
        asl a                   ; low byte = (row & 7) << 5
        sta $2006
        ldx #0
vblank_update_copy:
        lda $0300,x
        sta $2007
        inx
        cpx #32
        bne vblank_update_copy
        ; Restore scroll: the $2006/$2007 writes changed the PPU's internal
        ; v/t registers, which would corrupt the scroll with rendering on.
        lda $2002               ; reset latch
        lda #$00
        sta $2005               ; X scroll = 0
        sta $2005               ; Y scroll = 0
        lda #$80
        sta $2000               ; restore PPUCTRL (nametable bits in t = 0)
        rts
