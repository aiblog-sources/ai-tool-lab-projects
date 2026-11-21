; mul8: unsigned 8x8 -> 16 multiply
; In:  H, E = factors
; Out: HL = H * E
; Clobbers: D, F
mul8:
        ld      d,0             ; DE = multiplicand (zero-extended)
        ld      l,d             ; L = 0 (product accumulator low)

        ; bit 7 (first step: HL = H*256 with L=0, so just shift H)
        sla     h
        jr      nc,.b6
        ld      l,e             ; L was 0, so L = E is the same as HL += DE
.b6:    add     hl,hl
        jr      nc,.b5
        add     hl,de
.b5:    add     hl,hl
        jr      nc,.b4
        add     hl,de
.b4:    add     hl,hl
        jr      nc,.b3
        add     hl,de
.b3:    add     hl,hl
        jr      nc,.b2
        add     hl,de
.b2:    add     hl,hl
        jr      nc,.b1
        add     hl,de
.b1:    add     hl,hl
        jr      nc,.b0
        add     hl,de
.b0:    add     hl,hl
        ret     nc
        add     hl,de
        ret
