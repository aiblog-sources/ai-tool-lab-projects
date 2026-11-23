; reference: 1024 bytes with unrolled OUTI: the fastest plain copy, still too slow
vblank_update:
  ld hl,($C000)
  ld a,l
  out ($BF),a
  ld a,h
  or $40
  out ($BF),a
  ld hl,$C100
  ld c,$BE
  REPT 1024
  outi
  ENDR
  ret
