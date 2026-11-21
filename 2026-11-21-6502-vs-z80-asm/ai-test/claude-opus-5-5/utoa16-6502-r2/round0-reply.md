```asm
; utoa16: convert the unsigned 16-bit value in $00/$01 to 5 ASCII decimal
; digits, written at the pointer in ($02),y for y = 0..4.
; It uses repeated subtraction of 10000, 1000 and 100 with 16-bit math,
; then 10 with 8-bit math once the remainder is below 100.
; Scratch: $00/$01 (destroyed), A, X, Y.

utoa16:
        ; ---- 10000s ----
        ldx #'0'
        sec
@l0:    lda $00
        sbc #<10000
        tay
        lda $01
        sbc #>10000
        bcc @d0
        sta $01
        sty $00
        inx
        bne @l0                 ; always taken, carry still set
@d0:    txa
        ldy #0
        sta ($02),y

        ; ---- 1000s ----
        ldx #'0'
        sec
@l1:    lda $00
        sbc #<1000
        tay
        lda $01
        sbc #>1000
        bcc @d1
        sta $01
        sty $00
        inx
        bne @l1
@d1:    txa
        ldy #1
        sta ($02),y

        ; ---- 100s ----
        ldx #'0'
        sec
@l2:    lda $00
        sbc #<100
        tay
        lda $01
        sbc #>100
        bcc @d2
        sta $01
        sty $00
        inx
        bne @l2
@d2:    txa
        ldy #2
        sta ($02),y

        ; ---- 10s and 1s (remainder < 100, 8-bit) ----
        ldx #'0'
        lda $00
        sec
@l3:    sbc #10
        bcc @d3
        inx
        bcs @l3                 ; always taken
@d3:    adc #'0'+10             ; C=0: A = (rem-10)+'0'+10 = rem+'0'
        ldy #4
        sta ($02),y
        dey
        txa
        sta ($02),y
        rts
```

Worst-case cycles: 787

This assumes no branch crosses a page boundary. The worst case is the input 59999.