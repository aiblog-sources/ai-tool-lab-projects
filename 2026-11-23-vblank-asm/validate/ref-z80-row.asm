; reference: one name table row (64 bytes)
vblank_update:
  ld a,($C000)
  ld l,a
  ld h,0
  add hl,hl
  add hl,hl
  add hl,hl
  add hl,hl
  add hl,hl
  add hl,hl
  ld de,$7800
  add hl,de
  ld a,l
  out ($BF),a
  ld a,h
  out ($BF),a
  ld hl,$C100
  ld bc,$40BE
  otir
  ret
