; mul8: HL = H * E (unsigned 8x8 -> 16)
; Clobbers: D, flags
mul8:
        ld d,0
        ld l,d          ; L = 0, DE = E
        ; bit 7
        sla h
        jr nc,.b6
        ld l,e
.b6:    add hl,hl
        jr nc,.b5
        add hl,de
.b5:    add hl,hl
        jr nc,.b4
        add hl,de
.b4:    add hl,hl
        jr nc,.b3
        add hl,de
.b3:    add hl,hl
        jr nc,.b2
        add hl,de
.b2:    add hl,hl
        jr nc,.b1
        add hl,de
.b1:    add hl,hl
        jr nc,.b0
        add hl,de
.b0:    add hl,hl
        ret nc
        add hl,de
        ret
