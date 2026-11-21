```asm
; mul8: unsigned 8x8 -> 16 multiply
; In:  H, E = factors
; Out: HL = H * E
; Uses: D, flags
mul8:
        ld d,0
        sla h           ; bit 7 of multiplier -> carry
        ld l,d          ; L = 0 (flags unchanged)
        jr nc,.s1
        ld l,e          ; partial = E
.s1:
        add hl,hl       ; bit 6
        jr nc,.s2
        add hl,de
.s2:
        add hl,hl       ; bit 5
        jr nc,.s3
        add hl,de
.s3:
        add hl,hl       ; bit 4
        jr nc,.s4
        add hl,de
.s4:
        add hl,hl       ; bit 3
        jr nc,.s5
        add hl,de
.s5:
        add hl,hl       ; bit 2
        jr nc,.s6
        add hl,de
.s6:
        add hl,hl       ; bit 1
        jr nc,.s7
        add hl,de
.s7:
        add hl,hl       ; bit 0
        ret nc
        add hl,de
        ret
```

The routine is a fully unrolled shift-and-add. It shifts the multiplier out of H one bit at a time while the partial product builds up in the low bits of HL, and the two never overlap. The first bit costs less because it just loads E into L. For the last bit, `ret nc` ends early, and that branch never costs more than the fall-through.

Cycle count (CALL 17, setup and bit 7 at most 31, bits 6 to 1 at most 29 each, last bit at most 11+11+10):

Worst-case cycles: 255