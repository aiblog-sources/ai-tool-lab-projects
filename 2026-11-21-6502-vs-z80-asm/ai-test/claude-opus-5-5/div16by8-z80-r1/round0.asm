; div16by8: HL = HL / C, A = HL mod C (unsigned; C = 1..255)
; Restoring shift-and-subtract division, one quotient bit per pass.
; Each pass shifts the next dividend bit into A. When the bit
; shifted out of A is set, the partial remainder is 256 or more,
; so it is always >= C and C is subtracted.
; Uses: AF, B, HL.

div16by8:
        xor     a               ; remainder = 0, carry cleared
        ld      b,16            ; 16 quotient bits
.loop:
        add     hl,hl           ; shift dividend left, top bit -> carry
        rla                     ; carry into the remainder
        jr      c,.sub          ; remainder >= 256, so subtract C
        cp      c
        jr      c,.next         ; remainder < C: quotient bit is 0
.sub:
        sub     c               ; remainder -= C (correct modulo 256)
        inc     l               ; set quotient bit (bit 0 is 0 after the shift)
.next:
        djnz    .loop
        ret
