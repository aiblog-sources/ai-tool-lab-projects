vblank_update:
        lda #$00
        sta $2003               ; OAMADDR = 0
        lda #$02
        sta $4014               ; OAM DMA from $0200 (513/514 cycles)

        bit $2002               ; reset PPU write latch
        lda $01
        sta $2006               ; high byte of destination
        lda $00
        sta $2006               ; low byte of destination

        ; 160 bytes, fully unrolled: 8 cycles per byte
        .repeat 160, i
        lda $0300+i
        sta $2007
        .endrepeat

        bit $2002               ; reset write latch
        lda #$00
        sta $2005               ; scroll X = 0
        sta $2005               ; scroll Y = 0
        lda #$80
        sta $2000               ; PPUCTRL = $80 (also restores nametable bits in t)
        rts
