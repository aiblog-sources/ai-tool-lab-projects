```asm
; div16by8 - unsigned 16-bit / 8-bit division
; In : $00/$01 = dividend (lo/hi), $02 = divisor (1..255)
; Out: $03/$04 = quotient (lo/hi), $05 = remainder
; The dividend is copied into $03/$04 and shifted left one bit at a time.
; Each quotient bit enters at bit 0 of $03 as the dividend bits leave.
; A holds the running remainder.

div16by8:
        lda $00
        sta $03
        lda $01
        sta $04
        lda #0              ; remainder = 0
        ldx #16             ; 16 quotient bits
        asl $03             ; first dividend bit -> C, quotient bit 0 = 0
@loop:
        rol $04             ; carry from $03 -> $04, top dividend bit -> C
        rol a               ; shift it into the remainder (9-bit, overflow in C)
        bcs @big            ; remainder >= 256, so it is surely >= divisor
        cmp $02
        bcc @next           ; remainder < divisor: quotient bit = 0 (C=0)
        sbc $02             ; C=1 here, and it stays 1 (no borrow)
@next:
        rol $03             ; shift quotient bit in, next dividend bit -> C
        dex
        bne @loop
        rol $04             ; take in the final carry from $03
        sta $05             ; remainder
        rts

@big:
        sbc $02             ; C=1 in; the 9-bit result fits in 8 bits
        sec                 ; quotient bit = 1
        bcs @next           ; always taken
```

Worst-case cycles: 480