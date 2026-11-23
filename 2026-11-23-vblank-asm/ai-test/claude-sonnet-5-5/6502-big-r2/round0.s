vblank_update:
        lda #$02
        sta $4014               ; OAM DMA from $0200 (513/514 cycles stall)

        bit $2002               ; reset PPU address latch
        lda $01
        sta $2006
        lda $00
        sta $2006

        ; 160 bytes, fully unrolled: 8 cycles per byte
        .repeat 160, I
        lda $0300+I
        sta $2007
        .endrepeat

        ; restore scroll = (0,0) and PPUCTRL = $80
        bit $2002               ; reset write latch
        lda #$00
        sta $2005               ; X scroll = 0
        sta $2005               ; Y scroll = 0
        lda #$80
        sta $2000               ; NMI on, nametable $2000, increment 1
        rts
