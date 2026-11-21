; Test harness for one Z80 routine (sjasmplus). smsrun loads each test
; vector on the write to port $DE and reads the result on the write to $DF.
  OUTPUT "rom.sms"
  ORG 0
  di
  im 1
  ld sp,$DFF0
  jp main
  BLOCK $38-$,$FF
  ret                ; interrupts stay off
  BLOCK $66-$,$FF
  retn               ; pause button NMI
main:
  out ($DE),a
  call ROUTINE
  out ($DF),a
  jp main
  INCLUDE "routine.asm"
  ASSERT $ <= $7FF0
  BLOCK $7FF0-$,$FF
  DB "TMR SEGA",$FF,$FF,0,0,0,0,0,$4C
