```asm
; div16by8 - unsigned 16-bit / 8-bit division
; In:  $00/$01 = dividend (lo/hi), $02 = divisor (1..255)
; Out: $03/$04 = quotient (lo/hi), $05 = remainder
; The quotient bytes are first loaded with the dividend.
; Each pass shifts the top dividend bit into the remainder (A)
; and shifts a quotient bit into the bottom.

div16by8:
        lda $00
        sta $03
        lda $01
        sta $04
        lda #0                  ; remainder = 0
        ldx #16
@loop:
        asl $03                 ; shift dividend/quotient left
        rol $04
        rol a                   ; top bit goes into the remainder
        bcs @sub                ; 9-bit remainder >= 256 > divisor
        cmp $02
        bcc @skip               ; remainder < divisor: quotient bit = 0
@sub:
        sbc $02                 ; carry is set on both paths
        inc $03                 ; quotient bit = 1 (bit 0 was 0)
@skip:
        dex
        bne @loop
        sta $05
        rts
```
Worst-case cycles: 542