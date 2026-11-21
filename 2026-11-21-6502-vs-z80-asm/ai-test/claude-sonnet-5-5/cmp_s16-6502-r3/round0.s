cmp_s16:
        lda $00         ; a low
        cmp $02         ; set carry = borrow for the low byte
        lda $01         ; a high
        sbc $03         ; a - b (high bytes, with borrow)
        bvc @nv         ; no overflow: N is correct sign
        eor #$80        ; overflow: flip sign bit
@nv:    asl a           ; bit 7 (a<b) into carry
        rts
