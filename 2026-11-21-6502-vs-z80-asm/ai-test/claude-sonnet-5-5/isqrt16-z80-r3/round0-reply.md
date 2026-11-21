```asm
isqrt16:
        ld   d,h                ; DE = n (shifted out from the top)
        ld   e,l
        ld   hl,0               ; HL = remainder
        ld   bc,1               ; BC = trial value t = 4*root+1
        ld   a,8                ; 8 result bits
.loop:
        sla  e                  ; bring in the top 2 bits of n
        rl   d
        adc  hl,hl
        sla  e
        rl   d
        adc  hl,hl              ; remainder < 2048, so carry is clear here
        sbc  hl,bc              ; remainder - t
        jr   nc,.ok
        add  hl,bc              ; restore
        dec  bc                 ; t' = 2*(t-1)+1
        jr   .join
.ok:
        inc  bc                 ; t' = 2*(t+1)+1
.join:
        sla  c
        rl   b
        inc  c
        dec  a
        jr   nz,.loop
        srl  b                  ; t = 4*root+1, so root = t >> 2
        rr   c
        srl  b
        rr   c
        ld   a,c
        ret
```
Worst-case cycles: 1285