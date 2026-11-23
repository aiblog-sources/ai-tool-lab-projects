; vblank_update -- OAM DMA from $0200, then copy $0300-$03FF to PPU ($00/$01)
; Fully unrolled; 8 cycles per byte is the floor for RAM -> $2007.
; NOTE: total exceeds NTSC vblank (see notes) -- does not meet the spec.

vblank_update:
        lda #$00                ; 2
        sta $2003               ; 4   OAMADDR = 0
        lda #$02                ; 2
        sta $4014               ; 4 + 513/514  OAM DMA from $0200

        bit $2002               ; 4   reset $2005/$2006 write latch
        lda $01                 ; 3
        sta $2006               ; 4   PPUADDR high
        lda $00                 ; 3
        sta $2006               ; 4   PPUADDR low  (PPUCTRL still $80: +1)

        .repeat 256, I
        lda $0300+I             ; 4
        sta $2007               ; 4
        .endrepeat

        lda #$80                ; 2
        sta $2000               ; 4   PPUCTRL = $80, nametable $2000
        lda #$00                ; 2
        sta $2005               ; 4   scroll X = 0
        sta $2005               ; 4   scroll Y = 0
        rts
