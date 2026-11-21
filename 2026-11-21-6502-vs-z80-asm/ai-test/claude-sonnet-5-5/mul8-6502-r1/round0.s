; mul8: unsigned 8x8 -> 16 multiply
; In:  $00 = multiplier, $01 = multiplicand
; Out: $02 = product low, $03 = product high
; Clobbers A, X, flags, and $00 (scratch)

mul8:
        lda #0              ; A = product high accumulator
        ldx #8
        lsr $00             ; bit 0 of multiplier -> carry
@loop:
        bcc @skip           ; bit clear: no add
        clc
        adc $01             ; add multiplicand, carry out = bit 16 of partial
@skip:
        ror a               ; shift product right, carry in from add
        ror $00             ; low bits enter $00, next multiplier bit -> carry
        dex
        bne @loop
        sta $03
        lda $00
        sta $02
        rts
