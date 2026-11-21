; Branch-free popcount with a known cost: JSR 6 + 3 + 2 + 2 + 8 x (5 + 2) + RTS 6 = 75 cycles.
popcount:
  sta $00
  lda #0
  clc
  .repeat 8
  lsr $00
  adc #0
  .endrepeat
  rts
