```asm
; isqrt16: A = floor(sqrt(HL)), HL unsigned 16-bit
; Bit-by-bit restoring method, fully unrolled.
;   DE = n (shifted out 2 bits at a time)
;   HL = remainder
;   BC = T = 2*root (during a step: trial = T'+1)
; Uses no RAM.

isqrt16:
        ex de,hl            ; DE = n
        ld hl,0             ; remainder = 0
        ld bc,1             ; step 1: T' = 0, trial = 1

; ---- step 1 (T' = 0, so no shift needed)
        sla e
        rl d
        adc hl,hl
        sla e
        rl d
        adc hl,hl           ; carry = 0 here (HL small)
        sbc hl,bc
        jr nc,.s1
        add hl,bc
        dec c
        dec c
.s1:    inc c

; ---- steps 2..7
        sla e
        rl d
        adc hl,hl
        sla e
        rl d
        adc hl,hl
        sla c
        rl b                ; T' = 2T, carry = 0
        inc c               ; trial = T'+1
        sbc hl,bc
        jr nc,.s2
        add hl,bc
        dec c
        dec c
.s2:    inc c

        sla e
        rl d
        adc hl,hl
        sla e
        rl d
        adc hl,hl
        sla c
        rl b
        inc c
        sbc hl,bc
        jr nc,.s3
        add hl,bc
        dec c
        dec c
.s3:    inc c

        sla e
        rl d
        adc hl,hl
        sla e
        rl d
        adc hl,hl
        sla c
        rl b
        inc c
        sbc hl,bc
        jr nc,.s4
        add hl,bc
        dec c
        dec c
.s4:    inc c

        sla e
        rl d
        adc hl,hl
        sla e
        rl d
        adc hl,hl
        sla c
        rl b
        inc c
        sbc hl,bc
        jr nc,.s5
        add hl,bc
        dec c
        dec c
.s5:    inc c

        sla e
        rl d
        adc hl,hl
        sla e
        rl d
        adc hl,hl
        sla c
        rl b
        inc c
        sbc hl,bc
        jr nc,.s6
        add hl,bc
        dec c
        dec c
.s6:    inc c

        sla e
        rl d
        adc hl,hl
        sla e
        rl d
        adc hl,hl
        sla c
        rl b
        inc c
        sbc hl,bc
        jr nc,.s7
        add hl,bc
        dec c
        dec c
.s7:    inc c

; ---- step 8 (final): only the comparison result is needed
        sla e
        rl d
        adc hl,hl
        sla e
        rl d
        adc hl,hl
        sla c
        rl b                ; BC = T' = 2*root_new (<= 508)
        ld a,b
        rra
        ld a,c
        rra                 ; A = root_new, carry = 0 (T' even)
        inc c               ; trial = T'+1
        sbc hl,bc           ; carry = 1 if remainder < trial
        sbc a,$FF           ; A = A + 1 - carry
        ret
```
Worst-case cycles: 1044