; mul8: unsigned 8x8 -> 16 multiply
; In:  $00, $01 = factors
; Out: $02 = product low, $03 = product high
; Clobbers A, X, flags, $00

mul8:
        lda #0              ; high byte accumulator
        ldx #8
@loop:
        lsr $00             ; multiplier bit 0 -> carry
        bcc @noadd
        clc
        adc $01             ; add multiplicand into high accumulator
@noadd:
        ror a               ; shift product right (carry = add overflow)
        ror $02             ; low product bits shift in from A
        dex
        bne @loop
        sta $03
        rts
