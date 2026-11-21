```asm
utoa16:
        ldy #0
utoa16_digit:
        ldx #'0'
utoa16_sub:
        lda $00
        sec
        sbc utoa16_lo,y
        sta $04
        lda $01
        sbc utoa16_hi,y
        bcc utoa16_done        ; borrow: this power of ten no longer fits
        sta $01
        lda $04
        sta $00
        inx
        bne utoa16_sub         ; always taken (X is '0'..'9')
utoa16_done:
        txa
        sta ($02),y
        iny
        cpy #4
        bne utoa16_digit
        lda $00                ; remainder 0..9 is the ones digit
        ora #'0'
        sta ($02),y            ; Y = 4
        rts

utoa16_lo:
        .byte <10000, <1000, <100, <10
utoa16_hi:
        .byte >10000, >1000, >100, >10
```

Worst-case cycles: 1300