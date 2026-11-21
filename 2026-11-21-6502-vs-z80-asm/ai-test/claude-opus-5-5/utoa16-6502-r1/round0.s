; utoa16: convert the unsigned 16-bit value in $00/$01 into 5 ASCII digits
; written to ($02),y, most significant digit first, with leading zeros.
; Scratch: $00/$01 hold the remainder; registers A, X and Y are clobbered.

; One digit by repeated subtraction of a 16-bit power of ten.
; Expects carry set on entry and leaves carry set on exit,
; because the add-back always carries out of bit 15.
.macro utoa16_digit pow
        ldx #'0'-1          ; digit counter, ends at '0'+n
:       inx
        lda $00
        sbc #<pow
        sta $00
        lda $01
        sbc #>pow
        sta $01
        bcs :-              ; no borrow: try again
        lda $00             ; borrow: add pow back (carry clear here)
        adc #<pow
        sta $00
        lda $01
        adc #>pow
        sta $01             ; carry set after this add-back
        txa
        sta ($02),y
        iny
.endmacro

utoa16:
        ldy #0
        sec
        utoa16_digit 10000
        utoa16_digit 1000
        utoa16_digit 100
        ; remainder < 100, so the high byte is 0 and 8-bit math is enough
        ldx #'0'-1
        lda $00             ; carry is still set from the last add-back
@tens:  inx
        sbc #10
        bcs @tens
        adc #'0'+10         ; carry clear: add back 10 and make it ASCII
        ldy #4
        sta ($02),y         ; units digit
        dey
        txa
        sta ($02),y         ; tens digit
        rts
