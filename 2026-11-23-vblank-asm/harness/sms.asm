; VBlank test harness for one Z80 routine (sjasmplus, Master System).
; Init with the display off: VDP registers (mode 4, name table $3800, SAT
; $3F00), 256 tiles from tiles.bin, a name table pattern, palette, no
; sprites, and the case inputs ($C000-$C00F, buffer $C100-$C4FF) from
; case.asm. Then display and frame interrupt on, EI, idle loop. The IM 1
; handler marks its start and end with OUT ($DE) / OUT ($DF) (no device
; there on a Master System), saves every register including the shadow set,
; reads the VDP status (acknowledges the interrupt, resets the port latch),
; calls the routine, restores and returns. smsvb times every VDP write between
; the two markers.
  OUTPUT "rom.sms"
  ORG 0
  di
  im 1
  ld sp,$DFF0
  jp init
  BLOCK $38-$,$FF
isr:
  out ($DE),a
  push af
  push bc
  push de
  push hl
  push ix
  push iy
  ex af,af'
  push af
  exx
  push bc
  push de
  push hl
  in a,($BF)
  call ROUTINE
  pop hl
  pop de
  pop bc
  exx
  pop af
  ex af,af'
  pop iy
  pop ix
  pop hl
  pop de
  pop bc
  pop af
  out ($DF),a
  ei
  reti
  BLOCK $66-$,$FF
  retn

init:
  ld hl,vdp_regs
  ld b,11*2
  ld c,$BF
  otir
  ; tiles: VRAM $0000, 8 KB from tiles_data
  xor a
  out ($BF),a
  ld a,$40
  out ($BF),a
  ld hl,tiles_data
  ld de,8192
.tiles:
  ld a,(hl)
  out ($BE),a
  inc hl
  dec de
  ld a,d
  or e
  jr nz,.tiles
  ; name table $3800, 32x28 entries: low byte = index XOR $55, high byte 0
  xor a
  out ($BF),a
  ld a,$78
  out ($BF),a
  ld de,0
.nt:
  ld a,e
  xor $55
  out ($BE),a
  xor a
  out ($BE),a
  inc de
  ld a,d
  cp 3
  jr nz,.nt
  ; SAT: first Y = $D0 ends the sprite list
  xor a
  out ($BF),a
  ld a,$7F
  out ($BF),a
  ld a,$D0
  out ($BE),a
  ; palette
  xor a
  out ($BF),a
  ld a,$C0
  out ($BF),a
  ld hl,init_pal
  ld b,32
  ld c,$BE
  otir
  ; case inputs
  ld hl,case_args
  ld de,$C000
  ld bc,16
  ldir
  ld hl,case_buf
  ld de,$C100
  ld bc,1024
  ldir
  ; display on, frame interrupt on
  ld a,$E0
  out ($BF),a
  ld a,$81
  out ($BF),a
  in a,($BF)
  ei
idle:
  jr idle

vdp_regs:
  DB $04,$80, $A0,$81, $FF,$82, $FF,$83, $FF,$84, $FF,$85, $FB,$86, $00,$87, $00,$88, $00,$89, $FF,$8A
init_pal:
  DB $00,$03,$0C,$0F, $30,$33,$3C,$3F, $01,$02,$04,$08, $10,$20,$15,$2A
  DB $00,$3F,$03,$0C, $30,$05,$0A,$11, $22,$14,$28,$1B, $36,$2D,$1E,$39
tiles_data:
  INCBIN "tiles.bin"
  INCLUDE "case.asm"
  INCLUDE "routine.asm"
  ASSERT $ <= $7FF0
  BLOCK $7FF0-$,$FF
  DB "TMR SEGA",$FF,$FF,0,0,0,0,0,$4C
