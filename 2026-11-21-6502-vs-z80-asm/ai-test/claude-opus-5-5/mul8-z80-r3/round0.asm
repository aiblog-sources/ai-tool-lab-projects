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
