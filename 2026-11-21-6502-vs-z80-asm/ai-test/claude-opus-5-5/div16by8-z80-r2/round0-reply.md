```asm
; div16by8: unsigned HL / C
;   in : HL = dividend, C = divisor (1..255)
;   out: HL = quotient, A = remainder
;   uses: F
; Restoring shift-and-subtract division, fully unrolled (16 steps).
; For each step:
;   - Shift the next dividend bit from HL into A.
;   - Shift a 0 into HL as the new quotient bit.
;   - If carry is set after RLA, the partial remainder is >= 256, which is >= C.
;     Subtract C. The 8-bit wrap gives the correct result because it is < C.
;   - Otherwise subtract C only if A >= C.
;   - When we subtract, set the quotient bit with INC L.

div16by8:
        xor a                   ; remainder = 0
        rept 16
        add hl,hl               ; next dividend bit -> carry, quotient bit 0 = 0
        rla                     ; bring it into remainder
        jr c,$+5                ; overflowed 8 bits -> must subtract
        cp c
        jr c,$+4                ; A < C -> skip subtract
        sub c
        inc l                   ; quotient bit = 1
        endr
        ret
```

Worst-case cycles: 687