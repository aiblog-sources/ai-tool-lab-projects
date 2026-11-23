; vblank_update -- OAM DMA from $0200, then copy as much of $0300-$03FF to the
; PPU (address in $00/$01) as fits in NTSC vblank.
; Every unknown byte needs at least 8 cycles (LDA abs 4 + STA $2007 4), so the
; full 256 bytes (~2610 cycles with the DMA) cannot fit in vblank (~2263 cycles
; after the NMI handler starts). This copies the first 210 bytes and leaves all
; PPU writes inside vblank. Bytes $03D2-$03FF are NOT copied.

vblank_update_COUNT = 210

vblank_update:
        lda #$00                ; 2
        sta $2003               ; 4   OAMADDR = 0
        lda #$02                ; 2
        sta $4014               ; 4 + 513/514  OAM DMA from $0200

        bit $2002               ; 4   reset write latch
        lda $01                 ; 3
        sta $2006               ; 4   PPUADDR high
        lda $00                 ; 3
        sta $2006               ; 4   PPUADDR low (increment 1)

        .repeat vblank_update_COUNT, I
        lda $0300+I             ; 4
        sta $2007               ; 4
        .endrepeat

        lda #$80                ; 2
        sta $2000               ; 4   PPUCTRL = $80, nametable $2000
        lda #$00                ; 2
        sta $2005               ; 4   scroll X = 0
        sta $2005               ; 4   scroll Y = 0
        rts
