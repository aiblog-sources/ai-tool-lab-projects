; mul8: unsigned 8x8 -> 16 multiply
;   in:  $00 = factor A, $01 = factor B
;   out: $02 = product low, $03 = product high
;   scratch: $04
; Shift-add, fully unrolled. $00 shifts the multiplier bits out and
; collects the low bits of the product. A holds the high byte.
; Trick: the add happens only when C=1, so we add (B-1) with carry
; to get A+B and skip the CLC. B=0 is handled by an early exit.

mul8:
        lda #0
        ldx $01
        beq @zero               ; B = 0 -> product 0
        dex
        stx $04                 ; $04 = B-1 (C=1 at the add makes it +B)
        lsr $00                 ; C = bit 0 of the multiplier

        bcc @s1
        adc $04
@s1:    ror a
        ror $00

        bcc @s2
        adc $04
@s2:    ror a
        ror $00

        bcc @s3
        adc $04
@s3:    ror a
        ror $00

        bcc @s4
        adc $04
@s4:    ror a
        ror $00

        bcc @s5
        adc $04
@s5:    ror a
        ror $00

        bcc @s6
        adc $04
@s6:    ror a
        ror $00

        bcc @s7
        adc $04
@s7:    ror a
        ror $00

        bcc @s8
        adc $04
@s8:    ror a
        ror $00

        sta $03
        lda $00
        sta $02
        rts

@zero:  sta $02
        sta $03
        rts
