; reference: CRAM 0-31
vblank_update:
  xor a
  out ($BF),a
  ld a,$C0
  out ($BF),a
  ld hl,$C100
  ld bc,$20BE
  otir
  ret
