cmp_s16:
        lda $00         ; low byte compare, sets carry for the borrow
        cmp $02
        lda $01         ; high byte subtract with borrow
        sbc $03
        bvc @nov        ; no signed overflow: N is the correct "a < b"
        eor #$80        ; overflow: flip the sign bit
@nov:
        asl a           ; bit 7 into carry: set if a < b
        rts
