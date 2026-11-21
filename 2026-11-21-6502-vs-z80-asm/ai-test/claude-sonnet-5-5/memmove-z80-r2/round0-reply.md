```asm
; memmove: HL = src, DE = dst, BC = count
; If dst <= src, or the areas don't overlap, copy forward with LDIR.
; If dst > src, copy backward with LDDR, which is always safe in that case.
memmove:
        ld      a,b
        or      c
        ret     z               ; count = 0: nothing to copy
        push    hl
        or      a               ; clear carry
        sbc     hl,de           ; carry set if src < dst
        pop     hl              ; POP leaves the flags alone
        jr      c,.back
        ldir
        ret
.back:
        add     hl,bc
        dec     hl              ; HL = src+count-1
        ex      de,hl
        add     hl,bc
        dec     hl              ; HL = dst+count-1
        ex      de,hl
        lddr
        ret
```
Worst-case cycles: 1376364