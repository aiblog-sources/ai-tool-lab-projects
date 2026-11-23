; reference: one nametable row
vblank_update:
  bit $2002
  lda $00
  lsr a
  lsr a
  lsr a
  ora #$20
  sta $2006
  lda $00
  asl a
  asl a
  asl a
  asl a
  asl a
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
