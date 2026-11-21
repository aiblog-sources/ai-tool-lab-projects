Copy direction is chosen by comparing the addresses:

- **Forward (`LDIR`)** when `dst <= src`. Each byte is read before anything can overwrite it, so overlap is safe. `src == dst` also goes this way.
- **Backward (`LDDR`)** when `src < dst`. HL and DE are moved to the last byte of each area first.

A count of 0 returns at once, so nothing outside `dst..dst+count-1` is written. The routine uses no scratch RAM and pushes only one word onto the stack.

```asm
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
```

The worst case is the backward path with count = 65535:
- Code around the copy, including CALL and RET: 134 cycles.
- `LDDR`: 21×65534 + 16 = 1,376,230 cycles.

Worst-case cycles: 1376364