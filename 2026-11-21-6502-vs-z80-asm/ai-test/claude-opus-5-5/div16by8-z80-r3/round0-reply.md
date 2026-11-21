```asm
; div16by8: unsigned 16-bit / 8-bit division
; In:  HL = dividend, C = divisor (1..255)
; Out: HL = quotient, A = remainder
; Uses restoring shift-subtract, fully unrolled (16 steps).
; The remainder is shifted left into A. If RLA sets carry, the
; partial remainder has passed 255, so it is >= C and C is subtracted.
; SUB C still gives the right 8-bit result in that case.
div16by8:
        xor a                   ; remainder = 0
        REPT 16
        add hl,hl               ; shift dividend/quotient left, top bit -> CF
        rla                     ; shift that bit into the remainder
        jr c,$+5                ; 9-bit overflow -> must subtract (go to SUB C)
        cp c
        jr c,$+4                ; remainder < divisor -> skip (go past INC L)
        sub c                   ; remainder -= divisor
        inc l                   ; set quotient bit (bit 0 is 0 after ADD HL,HL)
        ENDR
        ret
```

Worst-case cycles: 687