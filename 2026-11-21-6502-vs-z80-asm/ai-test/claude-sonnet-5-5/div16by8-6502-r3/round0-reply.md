```asm
div16by8:
        lda #0
        sta $05             ; remainder = 0
        ldx #16
div16by8_loop:
        asl $00             ; shift dividend/quotient left, top bit -> carry
        rol $01
        rol $05             ; carry out = bit 8 of remainder
        lda $05             ; (LDA leaves carry untouched)
        bcs div16by8_sub    ; 9-bit remainder always >= divisor
        cmp $02
        bcc div16by8_next
div16by8_sub:
        sbc $02             ; carry is set on both paths
        sta $05
        inc $00             ; set quotient bit 0 (it is 0 after the ASL)
div16by8_next:
        dex
        bne div16by8_loop
        lda $00
        sta $03
        lda $01
        sta $04
        rts
```

Worst-case cycles: 686