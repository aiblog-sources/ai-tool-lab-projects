; reference: one nametable column with the +32 increment
vblank_update:
  lda #$84
  sta $2000
  bit $2002
  lda #$20
  sta $2006
  lda $00
  sta $2006
  ldx #0
@l:
  lda $0300,x
  sta $2007
  inx
  cpx #30
  bne @l
  lda #$80
  sta $2000
  lda #0
  sta $2005
  sta $2005
  rts
