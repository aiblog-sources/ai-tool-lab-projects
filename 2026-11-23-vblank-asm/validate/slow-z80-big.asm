; wrong on purpose: the same job with three OTIRs (21 cycles a byte)
vblank_update:
  ld hl,($C000)
  ld a,l
  out ($BF),a
  ld a,h
  or $40
  out ($BF),a
  ld hl,$C100
  ld bc,$00BE
  otir
  otir
  otir
  ret
