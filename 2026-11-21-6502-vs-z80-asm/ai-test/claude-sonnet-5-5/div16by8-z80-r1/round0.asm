div16by8:
        xor     a               ; remainder = 0
        ld      b,16
.loop:
        add     hl,hl           ; shift dividend MSB into carry, bit 0 of L = 0
        rla                     ; remainder = remainder*2 + that bit
        jr      c,.sub          ; 9-bit remainder overflowed, so it is >= divisor
        cp      c
        jr      c,.next         ; remainder < divisor
.sub:
        sub     c               ; (wraps correctly in the overflow case)
        inc     l               ; set quotient bit
.next:
        djnz    .loop
        ret
