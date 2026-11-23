; reference: one name table column, 24 address setups
vblank_update:
  ld a,($C000)
  add a,a
  ld e,a
  ld d,$78
  ld hl,$C100
  ld b,24
.l:
  ld a,e
  out ($BF),a
  ld a,d
  out ($BF),a
  ld a,(hl)
  out ($BE),a
  inc hl
  ld a,(hl)
  out ($BE),a
  inc hl
  ex de,hl
  ld a,64
  add a,l
  ld l,a
  adc a,h
  sub l
  ld h,a
  ex de,hl
  djnz .l
  ret
