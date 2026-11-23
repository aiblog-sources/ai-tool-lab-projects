VBLANK_UPDATE_COPY_COUNT = 200      ; max that fits in vblank (256 does not)

vblank_update:
        ; --- OAM DMA from $0200 ---
        lda #$00
        sta $2003
        lda #$02
        sta $4014               ; 513/514 cycles

        ; --- set PPU address from $00/$01 ---
        bit $2002               ; reset write latch
        lda $01
        sta $2006
        lda $00
        sta $2006

        ; --- unrolled copy, 8 cycles/byte ---
        .repeat VBLANK_UPDATE_COPY_COUNT, i
        lda $0300+i
        sta $2007
        .endrepeat

        ; --- restore scroll and PPUCTRL ---
        bit $2002               ; reset write latch
        lda #$00
        sta $2005               ; X scroll = 0
        sta $2005               ; Y scroll = 0
        lda #$80
        sta $2000               ; nametable $2000, NMI on, inc 1
        rts
