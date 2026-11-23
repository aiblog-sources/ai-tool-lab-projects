vblank_update:
        lda #$00
        sta $2003               ; OAMADDR = 0
        lda #$02
        sta $4014               ; OAM DMA from $0200 (513/514 cycles)

        lda $01                 ; PPUADDR high
        sta $2006
        lda $00                 ; PPUADDR low
        sta $2006

        ; Unrolled copy, 8 cycles/byte (LDA abs = 4, STA $2007 = 4).
        ; Only the first 200 bytes fit in vblank (see notes below).
        .repeat 200, i
        lda $0300+i
        sta $2007
        .endrepeat

        lda #$80                ; PPUCTRL = $80, which also sets the nametable bits in t to 0
        sta $2000
        lda #$00
        sta $2005               ; scroll X = 0
        sta $2005               ; scroll Y = 0
        rts
