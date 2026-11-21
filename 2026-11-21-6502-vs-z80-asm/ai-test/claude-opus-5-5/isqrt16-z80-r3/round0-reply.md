```asm
; isqrt16: A = floor(sqrt(HL)), HL unsigned 16-bit
; Restoring bit-by-bit method, fully unrolled.
;   DE = n (bits shifted out of the top, two per step)
;   HL = remainder
;   BC = T = 2*root (the trial value is 2*T+1)
isqrt16:
        ex de,hl            ; DE = n
        ld hl,0             ; rem = 0
        ld b,h
        ld c,1              ; step 1: T=0, so trial = 1

; ---- step 1 ----
        sla e
        rl d
        adc hl,hl
        sla e
        rl d
        adc hl,hl           ; rem <= 3, so carry is clear
        sbc hl,bc
        jr nc,isqrt16_s1
        add hl,bc
        dec c
isqrt16_r1:
; ---- step 2 ----
        sla e
        rl d
        adc hl,hl
        sla e
        rl d
        adc hl,hl
        sla c
        rl b                ; B <= 1, so carry is now clear
        inc c               ; trial = 2T+1 (inc keeps carry)
        sbc hl,bc
        jr nc,isqrt16_s2
        add hl,bc
        dec c
isqrt16_r2:
; ---- step 3 ----
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
        jr nc,isqrt16_s3
        add hl,bc
        dec c
isqrt16_r3:
; ---- step 4 ----
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
        jr nc,isqrt16_s4
        add hl,bc
        dec c
isqrt16_r4:
; ---- step 5 ----
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
        jr nc,isqrt16_s5
        add hl,bc
        dec c
isqrt16_r5:
; ---- step 6 ----
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
        jr nc,isqrt16_s6
        add hl,bc
        dec c
isqrt16_r6:
; ---- step 7 ----
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
        jr nc,isqrt16_s7
        add hl,bc
        dec c
isqrt16_r7:
; ---- step 8 (final) ----
        ld a,c              ; A = T = 2*root7 (<= 254)
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
        ret c               ; no success: root = T
        inc a               ; success: root = T+1
        ret

; ---- success paths: T = trial+1 = 2T+2 ----
isqrt16_s1:
        inc bc
        jr isqrt16_r1
isqrt16_s2:
        inc bc
        jr isqrt16_r2
isqrt16_s3:
        inc bc
        jr isqrt16_r3
isqrt16_s4:
        inc bc
        jr isqrt16_r4
isqrt16_s5:
        inc bc
        jr isqrt16_r5
isqrt16_s6:
        inc bc
        jr isqrt16_r6
isqrt16_s7:
        inc bc
        jr isqrt16_r7
```

Worst-case cycles: 1031