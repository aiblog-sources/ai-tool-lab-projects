vblank_update:
        lda #$00
        sta $2003               ; OAMADDR = 0
        lda #$02
        sta $4014               ; OAM DMA from $0200 (513/514 cycles)

        lda $01                 ; PPUADDR high
        sta $2006
        lda $00                 ; PPUADDR low
        sta $2006

        ; Unrolled copy at 8 cycles/byte (LDA abs = 4, STA $2007 = 4).
        ; This is the fastest possible: the source is in RAM, so every byte
        ; needs a 4-cycle load and a 4-cycle PPU write.
        ; Only the first 208 bytes fit in vblank.
        .repeat 208, i
        lda $0300+i
        sta $2007
        .endrepeat

        lda #$80                ; PPUCTRL = $80
        sta $2000
        lda #$00
        sta $2005               ; scroll X = 0
        sta $2005               ; scroll Y = 0
        rts
