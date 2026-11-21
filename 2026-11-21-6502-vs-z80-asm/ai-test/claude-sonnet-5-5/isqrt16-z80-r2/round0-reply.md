```asm
isqrt16:
        ex de,hl            ; DE = n (shifted out two bits at a time)
        ld hl,0             ; HL = remainder
        ld bc,1             ; BC = trial value T = 4*root+1
        DUP 8
        sla e               ; shift top 2 bits of n into remainder
        rl d
        adc hl,hl
        sla e
        rl d
        adc hl,hl
        or a
        sbc hl,bc           ; rem - T
        jr c,1F
        sla c               ; success: T' = 2T+3
        rl b
        dec c
        set 2,c
        jr 2F
1:      add hl,bc           ; fail: restore remainder, T' = 2T-1
        sla c
        rl b
        dec c
2:
        EDUP
        ld a,c              ; root = T >> 2
        srl b
        rr a
        srl b
        rr a
        ret
```
Worst-case cycles: 1111