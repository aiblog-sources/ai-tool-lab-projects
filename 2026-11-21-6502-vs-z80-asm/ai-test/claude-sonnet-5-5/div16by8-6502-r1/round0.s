; div16by8: unsigned 16-bit / 8-bit division
; In:  $00/$01 = dividend (lo/hi), $02 = divisor (1..255)
; Out: $03/$04 = quotient (lo/hi), $05 = remainder
; The dividend is shifted left in place, and quotient bits enter at bit 0 of $00.

div16by8:
        lda #0
        sta $05             ; remainder = 0
        ldx #16
@loop:
        asl $00             ; shift dividend/quotient left
        rol $01
        rol $05             ; shift top bit into remainder; carry = 9th bit
        bcs @sub            ; 9-bit remainder is always >= divisor
        lda $05
        cmp $02
        bcc @next           ; remainder < divisor: quotient bit stays 0
@sub:
        lda $05
        sbc $02             ; carry is set on both paths
        sta $05
        inc $00             ; set quotient bit (bit 0 is 0 after the asl)
@next:
        dex
        bne @loop

        lda $00
        sta $03
        lda $01
        sta $04
        rts
