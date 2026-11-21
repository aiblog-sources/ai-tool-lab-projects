popcount:
  cmp #$80
  beq popcount       ; hangs when A = $80
  lda #0
  rts
