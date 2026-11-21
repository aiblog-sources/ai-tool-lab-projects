```asm
; div16by8: unsigned 16-bit / 8-bit division
; In:  $00/$01 = dividend (lo/hi), $02 = divisor (1..255)
; Out: $03/$04 = quotient (lo/hi), $05 = remainder
div16by8:
        lda $00
        sta $03             ; quotient is built in place of the dividend
        lda $01
        sta $04
        lda #0              ; A = running remainder
        ldx #16
@loop:
        asl $03             ; shift dividend/quotient left
        rol $04
        rol a               ; bring the next bit into the remainder
        bcs @sub            ; 9-bit remainder, so it is surely >= divisor (C=1)
        cmp $02
        bcc @skip           ; remainder < divisor
@sub:
        sbc $02             ; C=1 here, so this is a plain subtract
        inc $03             ; set quotient bit 0 (it was 0 after the ASL)
@skip:
        dex
        bne @loop
        sta $05             ; remainder
        rts
```

Worst-case cycles: 542