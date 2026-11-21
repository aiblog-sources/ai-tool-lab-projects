; Branch-free popcount with a known cost: CALL 17 + 4 + 4 + 8 x (8 + 7) + RET 10 = 155 cycles.
popcount:
  ld c,a
  xor a
  REPT 8
  rr c
  adc a,0
  ENDR
  ret
