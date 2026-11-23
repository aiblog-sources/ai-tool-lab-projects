; reference: palette copy, then scroll and PPUCTRL reset
vblank_update:
  bit $2002
  lda #$3F
  sta $2006
  lda #0
  sta $2006
  ldx #0
@l:
  lda $0300,x
  sta $2007
  inx
  cpx #32
  bne @l
  lda #0
  sta $2005
  sta $2005
  lda #$80
  sta $2000
  rts
