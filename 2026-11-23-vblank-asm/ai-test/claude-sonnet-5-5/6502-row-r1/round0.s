vblank_update:
        bit $2002               ; reset the PPU write latch
        lda $00                 ; row 0-29
        lsr a
        lsr a
        lsr a                   ; row >> 3
        ora #$20                ; high byte = $20 + row/8
        sta $2006
        lda $00
        asl a
        asl a
        asl a
        asl a
        asl a                   ; low byte = (row & 7) << 5
        sta $2006
        ldx #0
@loop:
        lda $0300,x
        sta $2007
        inx
        cpx #32
        bne @loop
        lda #$80
        sta $2000               ; PPUCTRL = $80 (nametable $2000, increment 1)
        lda #0
        sta $2005               ; scroll X = 0
        sta $2005               ; scroll Y = 0
        rts
