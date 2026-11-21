; mul8 - unsigned 8x8 -> 16-bit multiply
; In:  $00 = multiplier, $01 = multiplicand
; Out: $02 = product low byte, $03 = product high byte
; Clobbers A and flags. $00 and $01 are left unchanged.
;
; Shift-and-add, fully unrolled. $02 holds the multiplier and
; collects the low byte of the product at the same time. Each
; ROR $02 shifts the next product bit in at the top and pushes
; the next multiplier bit out into the carry.

mul8:
        lda $00
        lsr a              ; C = multiplier bit 0
        sta $02            ; $02 = 0 m7..m1
        lda #0             ; A = running high byte
.repeat 8
        bcc :+
        clc
        adc $01            ; add multiplicand; carry out kept for ROR
:       ror a              ; carry -> A bit 7, A bit 0 -> C
        ror $02            ; C -> product low bit 7, next multiplier bit -> C
.endrepeat
        sta $03
        rts
