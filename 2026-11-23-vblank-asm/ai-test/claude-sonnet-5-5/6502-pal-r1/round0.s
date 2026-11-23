vblank_update:
        lda $2002               ; reset PPU address latch
        lda #$3F
        sta $2006
        lda #$00
        sta $2006               ; PPUADDR = $3F00
        ldx #0
@loop:
        lda $0300,x
        sta $2007
        inx
        cpx #32
        bne @loop

        lda #$3F                ; point v at $3F00, then $0000, so the
        sta $2006               ; palette address isn't left on the bus
        lda #$00                ; while rendering is on
        sta $2006
        sta $2006
        sta $2006               ; v = $0000
        sta $2005               ; scroll X = 0
        sta $2005               ; scroll Y = 0
        lda #$80
        sta $2000               ; PPUCTRL = $80, nametable $2000
        rts
