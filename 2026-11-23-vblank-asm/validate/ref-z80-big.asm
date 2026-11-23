; reference: 768 bytes with unrolled OUTI (16 cycles a byte)
vblank_update:
  ld hl,($C000)
  ld a,l
  out ($BF),a
  ld a,h
  or $40
  out ($BF),a
  ld hl,$C100
  ld c,$BE
  REPT 768
  outi
  ENDR
  ret
