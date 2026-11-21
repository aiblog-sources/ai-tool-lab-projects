```asm
isqrt16:
        ex      de,hl           ; DE = n (shifted out 2 bits per step)
        ld      hl,0            ; HL = remainder
        ld      bc,1            ; BC = trial = 4*root+1 (root = 0)
        REPT 8
        sla     e               ; top bit of n -> carry
        rl      d
        adc     hl,hl           ; shift into remainder
        sla     e
        rl      d
        adc     hl,hl           ; carry is 0 after this (remainder < 2^15)
        sbc     hl,bc           ; remainder - trial
        jr      c,1F
        sla     c               ; success: new trial = 2t+3
        rl      b
        inc     c               ; low bits were 010, so +3 cannot carry
        inc     c
        inc     c
        jr      2F
1:      add     hl,bc           ; failure: restore remainder
        sla     c               ; new trial = 2t-1
        rl      b
        dec     c               ; low bits were 010, so -1 cannot borrow
2:
        ENDR
        ld      a,c             ; root = trial >> 2
        srl     b
        rra
        srl     b
        rra
        ret
```
Worst-case cycles: 1071