```asm
div16by8:
        lda #0
        sta $05             ; remainder = 0
        ldx #16             ; 16 dividend bits
@loop:
        asl $00             ; shift dividend left, MSB -> carry
        rol $01
        rol $05             ; carry = bit shifted out of the 8-bit remainder
        lda $05             ; LDA leaves carry intact
        bcs @sub            ; 9-bit remainder >= 256 > divisor, so subtract
        cmp $02
        bcc @next           ; remainder < divisor
@sub:
        sbc $02             ; carry is set on both paths here
        sta $05
        inc $00             ; bit 0 is 0 after ASL, so this sets the quotient bit
@next:
        dex
        bne @loop

        lda $00             ; quotient now lives in $00/$01
        sta $03
        lda $01
        sta $04
        rts
```
Worst-case cycles: 686