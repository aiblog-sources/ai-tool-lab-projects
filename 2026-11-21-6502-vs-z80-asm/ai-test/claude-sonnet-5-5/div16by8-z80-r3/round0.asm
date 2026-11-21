; div16by8: unsigned 16-bit / 8-bit division
; In:  HL = dividend, C = divisor (1..255)
; Out: HL = quotient, A = remainder
; Clobbers: B, F
div16by8:
        xor     a               ; remainder = 0
        ld      b,16
.loop:
        add     hl,hl           ; shift dividend left, top bit -> carry
        rla                     ; shift it into remainder (carry out = 9th bit)
        jr      c,.sub          ; remainder >= 256, so it is certainly >= divisor
        cp      c
        jr      c,.next         ; remainder < divisor
.sub:
        sub     c               ; 8-bit wraparound gives the correct result
        inc     l               ; set quotient bit (bit 0 is 0 after the shift)
.next:
        djnz    .loop
        ret
