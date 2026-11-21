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
        adc hl,hl           ; rem <= 3, carry clear
        sbc hl,bc
        jr c,.f1
        inc c               ; success: T = trial+1
        jr .n1
.f1:    add hl,bc
        dec c               ; fail: T = trial-1
.n1:
; ---- step 2 ----
        sla e
        rl d
        adc hl,hl
        sla e
        rl d
        adc hl,hl
        sla c               ; C <= 126, carry clear, B stays 0
        inc c
        sbc hl,bc
        jr c,.f2
        inc c
        jr .n2
.f2:    add hl,bc
        dec c
.n2:
; ---- step 3 ----
        sla e
        rl d
        adc hl,hl
        sla e
        rl d
        adc hl,hl
        sla c
        inc c
        sbc hl,bc
        jr c,.f3
        inc c
        jr .n3
.f3:    add hl,bc
        dec c
.n3:
; ---- step 4 ----
        sla e
        rl d
        adc hl,hl
        sla e
        rl d
        adc hl,hl
        sla c
        inc c
        sbc hl,bc
        jr c,.f4
        inc c
        jr .n4
.f4:    add hl,bc
        dec c
.n4:
; ---- step 5 ----
        sla e
        rl d
        adc hl,hl
        sla e
        rl d
        adc hl,hl
        sla c
        inc c
        sbc hl,bc
        jr c,.f5
        inc c
        jr .n5
.f5:    add hl,bc
        dec c
.n5:
; ---- step 6 ----
        sla e
        rl d
        adc hl,hl
        sla e
        rl d
        adc hl,hl
        sla c
        inc c
        sbc hl,bc
        jr c,.f6
        inc c
        jr .n6
.f6:    add hl,bc
        dec c
.n6:
; ---- step 7 ----
        sla e
        rl d
        adc hl,hl
        sla e
        rl d
        adc hl,hl
        sla c
        inc c
        sbc hl,bc
        jr c,.f7
        inc c
        jr .n7
.f7:    add hl,bc
        dec c
.n7:
; ---- step 8 (final) ----
        ld a,c              ; A = T = 2*root7 (<= 254)
        sla e
        rl d
        adc hl,hl
        sla e
        rl d
        adc hl,hl           ; rem <= 1019, carry clear
        sla c
        rl b                ; BC = 2T (B was 0, carry now clear)
        inc c               ; trial = 2T+1
        sbc hl,bc
        ret c               ; fail: root = T
        inc a               ; success: root = T+1
        ret
