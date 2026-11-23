; vblank_update -- OAM DMA from $0200, then copy $0300-$03FF to PPU ($00/$01)
; This is already the minimum possible: 8 cycles per byte (STA $2007 = 4, and no
; load from $0300 page is cheaper than 4). 256 bytes + OAM DMA ~= 2614 cycles,
; but NTSC vblank is ~2273 cycles. Only ~210 bytes fit after the DMA.
; Meeting the spec needs a rule change (split across frames with a persistent
; RAM byte, smaller transfer, or forced blank).

vblank_update:
        lda #$00                ; 2
        sta $2003               ; 4   OAMADDR = 0
        lda #$02                ; 2
        sta $4014               ; 4 + 513/514  OAM DMA from $0200

        bit $2002               ; 4   reset $2005/$2006 write latch
        lda $01                 ; 3
        sta $2006               ; 4   PPUADDR high
        lda $00                 ; 3
        sta $2006               ; 4   PPUADDR low  (PPUCTRL $80: increment 1)

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
