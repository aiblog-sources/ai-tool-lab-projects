VBLANK_UPDATE_COPY_COUNT = 208      ; max that safely fits in NTSC vblank (256 cannot)

vblank_update:
        ; --- OAM DMA from $0200 ---
        lda #$00
        sta $2003
        lda #$02
        sta $4014               ; 513/514 cycles of DMA

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

        ; --- restore scroll and PPUCTRL (write latch is already clear
        ;     after the two $2006 writes; $2007 does not touch it) ---
        lda #$00
        sta $2005               ; X scroll = 0
        sta $2005               ; Y scroll = 0
        lda #$80
        sta $2000               ; nametable $2000, NMI on, inc 1
        rts
