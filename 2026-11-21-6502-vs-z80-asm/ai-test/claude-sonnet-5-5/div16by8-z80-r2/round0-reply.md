```asm
div16by8:
        xor     a               ; remainder = 0
        ld      b,16            ; 16 dividend bits
.loop:
        add     hl,hl           ; shift dividend MSB out, quotient bit in (bit 0 = 0)
        rla                     ; shift it into remainder; carry = 9th bit
        jr      c,.sub          ; remainder overflowed 8 bits, so it is >= C
        cp      c
        jr      c,.next         ; remainder < divisor, no subtract
.sub:
        sub     c               ; 8-bit wraparound is correct when carry was set
        inc     l               ; quotient bit = 1
.next:
        djnz    .loop
        ret
```
Worst-case cycles: 897