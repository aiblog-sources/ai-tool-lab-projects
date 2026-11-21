memmove:
        ld      a,b
        or      c
        ret     z               ; count = 0
        ld      a,h
        cp      d
        jr      c,.back         ; src < dst (high byte)
        jr      nz,.fwd         ; src > dst
        ld      a,l
        cp      e
        jr      c,.back         ; src < dst (low byte)
.fwd:
        ldir                    ; dst <= src: forward copy is safe
        ret
.back:
        add     hl,bc
        dec     hl              ; HL = src+count-1
        ex      de,hl
        add     hl,bc
        dec     hl              ; HL = dst+count-1
        ex      de,hl
        lddr                    ; dst > src: backward copy is safe
        ret
