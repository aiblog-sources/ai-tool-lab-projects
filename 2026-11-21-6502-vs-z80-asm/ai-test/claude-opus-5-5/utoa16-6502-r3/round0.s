; utoa16: convert the unsigned 16-bit value in $00/$01 to 5 ASCII digits,
; written at ($02),y for y = 0..4, most significant digit first.
; Method: repeated subtraction of 10000, 1000, 100 and 10. The last digit is
; what remains. Scratch: $04 = digit character, $05 = trial low byte.
utoa16:
        ldy #0                  ; output index
        ldx #0                  ; power-of-ten table index
@digit:
        lda #'0'
        sta $04                 ; digit character starts at '0'
@loop:
        lda $00
        sec
        sbc utoa16_lo,x
        sta $05                 ; trial low byte
        lda $01
        sbc utoa16_hi,x         ; trial high byte
        bcc @done               ; borrow: value < power, digit finished
        sta $01                 ; commit the subtraction
        lda $05
        sta $00
        inc $04                 ; next digit character
        bne @loop               ; always taken
@done:
        lda $04
        sta ($02),y
        iny
        inx
        cpx #4
        bne @digit
        lda $00                 ; remainder 0..9
        ora #'0'
        sta ($02),y
        rts

utoa16_lo:
        .byte <10000, <1000, <100, <10
utoa16_hi:
        .byte >10000, >1000, >100, >10
