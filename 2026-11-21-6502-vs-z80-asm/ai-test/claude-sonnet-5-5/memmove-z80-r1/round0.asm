memmove:
        ld      a,b
        or      c
        ret     z               ; count = 0
        or      a
        sbc     hl,de           ; src - dst
        jr      nc,.fwd         ; src >= dst: forward copy is safe
        add     hl,de           ; restore src (src < dst)
        push    hl
        add     hl,bc           ; src + count
        jr      c,.back_pop     ; beyond 64K, so it overlaps dst
        or      a
        sbc     hl,de           ; src + count - dst
        jr      c,.fwd_pop      ; no overlap
        jr      z,.fwd_pop      ; adjacent, no overlap
.back_pop:
        pop     hl
        add     hl,bc
        dec     hl              ; src + count - 1
        ex      de,hl
        add     hl,bc
        dec     hl              ; dst + count - 1
        ex      de,hl
        lddr
        ret
.fwd_pop:
        pop     hl
        ldir
        ret
.fwd:
        add     hl,de           ; restore src
        ldir
        ret
