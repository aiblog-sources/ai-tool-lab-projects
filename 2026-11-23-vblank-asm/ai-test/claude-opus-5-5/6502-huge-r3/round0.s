; vblank_update -- OAM DMA, then copy $0300-$03FF to VRAM at ($00/$01),
; then restore PPUCTRL=$80 and scroll 0,0.
; WARNING: about 2604 cycles from JSR; NTSC vblank allows about 2253 after
; NMI overhead. The tail of the VRAM copy will land in the pre-render line.
vblank_update:
        lda #$02                ; 2
        sta $4014               ; 4 + 513/514  OAM DMA from $0200

        lda $01                 ; 3  dest high
        sta $2006               ; 4
        lda $00                 ; 3  dest low
        sta $2006               ; 4  (w latch back to 0 after two writes)

        .repeat 256, I          ; fully unrolled: 8 cycles per byte
        lda $0300+I             ; 4
        sta $2007               ; 4
        .endrepeat

        lda #$80                ; 2
        sta $2000               ; 4  NMI on, nametable $2000, inc 1
        lda #$00                ; 2
        sta $2005               ; 4  scroll X = 0
        sta $2005               ; 4  scroll Y = 0
        rts
