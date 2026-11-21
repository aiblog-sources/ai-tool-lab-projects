; memmove: copy BC bytes from HL (src) to DE (dst), overlap-safe.
memmove:
        ld      a,b
        or      c
        ret     z               ; count = 0: nothing to do

        push    hl
        or      a               ; clear carry
        sbc     hl,de           ; src - dst, carry set if src < dst
        pop     hl
        jr      c,.backward     ; src < dst: copy from the end

        ldir                    ; dst <= src: forward copy is safe
        ret

.backward:
        add     hl,bc
        dec     hl              ; HL = src + count - 1
        ex      de,hl
        add     hl,bc
        dec     hl              ; HL = dst + count - 1
        ex      de,hl
        lddr                    ; copy from the end down
        ret
