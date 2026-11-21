; Test harness for one 6502 routine (ca65). nesrun loads each test vector
; when the CPU reaches `call` and reads the result at `after`.
.segment "HEADER"
  .byte "NES", $1A, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
.segment "CODE"
reset:
  sei
  cld
  ldx #$FF
  txs
  lda #0
  sta $2000          ; no NMI
  sta $2001          ; rendering off
call:
  jsr ROUTINE
after:
  jmp call
nmi:
irq:
  rti
.include "routine.s"
.segment "VECTORS"
  .word nmi, reset, irq
