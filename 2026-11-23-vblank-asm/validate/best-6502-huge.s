; reference: OAM DMA, then 256 bytes fully unrolled: the fastest plain copy, still too slow (8 cycles a byte)
vblank_update:
  lda #0
  sta $2003
  lda #$02
  sta $4014
  bit $2002
  lda $01
  sta $2006
  lda $00
  sta $2006
.repeat 256, I
  lda $0300+I
  sta $2007
.endrepeat
  lda #0
  sta $2005
  sta $2005
  lda #$80
  sta $2000
  rts
