; VBlank test harness for one 6502 routine (ca65, NROM, CHR RAM).
; Init with rendering off: CHR RAM from chr.bin, both nametables filled with
; a known pattern, palette, OAM page $0200 and the case inputs ($00-$0F and
; the buffer $0300-$03FF) from case.s. Then NMI on ($80), rendering on ($1E),
; scroll 0,0, and an idle loop. The NMI handler saves A X Y, calls the
; routine, restores them and returns. nesvb times every PPU write between
; `nmi` and `nmi_exit`.
.segment "HEADER"
  .byte "NES", $1A, 2, 0, $01, 0, 0, 0, 0, 0, 0, 0, 0, 0   ; vertical mirroring

.segment "ZEROPAGE"
.segment "CODE"
reset:
  sei
  cld
  ldx #$FF
  txs
  lda #0
  sta $2000
  sta $2001
  bit $2002
@w1: bit $2002
  bpl @w1
@w2: bit $2002
  bpl @w2
  ; CHR RAM $0000-$0FFF from chr_data (4 KB), then $1000-$1FFF the same
  lda #0
  sta $2006
  sta $2006
  ldy #2
@chr_twice:
  lda #<chr_data
  sta $10
  lda #>chr_data
  sta $11
  ldx #16
@chr_page:
  tya
  pha
  ldy #0
@chr_byte:
  lda ($10),y
  sta $2007
  iny
  bne @chr_byte
  pla
  tay
  inc $11
  dex
  bne @chr_page
  dey
  bne @chr_twice
  ; nametables $2000-$27FF: byte = low byte of its offset XOR $55
  lda #$20
  sta $2006
  lda #0
  sta $2006
  ldx #8
  ldy #0
@nt:
  tya
  eor #$55
  sta $2007
  iny
  bne @nt
  dex
  bne @nt
  ; palette
  lda #$3F
  sta $2006
  lda #0
  sta $2006
  ldx #0
@pal:
  lda init_pal,x
  sta $2007
  inx
  cpx #32
  bne @pal
  ; case inputs: $00-$0F, OAM page $0200, buffer $0300
  ldx #0
@args:
  lda case_args,x
  sta $00,x
  inx
  cpx #16
  bne @args
  ldx #0
@pages:
  lda case_oam,x
  sta $0200,x
  lda case_buf,x
  sta $0300,x
  inx
  bne @pages
  ; scratch $10-$1F cleared
  lda #0
  ldx #15
@clr:
  sta $10,x
  dex
  bpl @clr
  ; one OAM DMA so OAM starts from case_oam in every task, then page $0200
  ; gets case_oam2 (different from case_oam only in the task with OAM DMA)
  lda #$02
  sta $4014
  ldx #0
@oam2:
  lda case_oam2,x
  sta $0200,x
  inx
  bne @oam2
@w3: bit $2002
  bpl @w3
  lda #0
  sta $2005
  sta $2005
  lda #$80
  sta $2000
  lda #$1E
  sta $2001
idle:
  jmp idle

nmi:
  pha
  txa
  pha
  tya
  pha
  jsr ROUTINE
  pla
  tay
  pla
  tax
  pla
nmi_exit:
  rti
irq:
  rti

.include "routine.s"

.segment "RODATA"
chr_data: .incbin "chr.bin"
init_pal:
  .byte $0F,$01,$11,$21, $0F,$06,$16,$26, $0F,$09,$19,$29, $0F,$02,$12,$22
  .byte $0F,$04,$14,$24, $0F,$07,$17,$27, $0F,$0A,$1A,$2A, $0F,$0C,$1C,$2C
.include "case.s"

.segment "VECTORS"
  .word nmi, reset, irq
