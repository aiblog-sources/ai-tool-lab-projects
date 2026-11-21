```asm
; memmove: copy BC bytes from HL (src) to DE (dst), overlap-safe.
; If src >= dst, a forward copy (LDIR) is safe.
; If src < dst, a backward copy (LDDR) from the last byte is always safe.
memmove:
        ld      a,b
        or      c
        ret     z               ; count = 0: nothing to do

        push    hl
        or      a               ; clear carry
        sbc     hl,de           ; carry set if src < dst
        jr      nc,.fwd

        ; src < dst: copy backward from the end
        pop     hl
        add     hl,bc
        dec     hl              ; HL = src + count - 1
        ex      de,hl
        add     hl,bc
        dec     hl              ; HL = dst + count - 1
        ex      de,hl
        lddr
        ret

.fwd:   ; src >= dst: copy forward
        pop     hl
        ldir
        ret
```

Worst-case cycles: 1376359