The only error was the `JR C` at line 19. The forward copy block is more than 127 bytes away, so `JR` cannot reach it. I replaced it with `JP C`, which has no range limit.

```asm
; memmove: HL = src, DE = dst, BC = count (0..65535)
; Copies forward when dst < src and backward otherwise, so it is correct
; whether the two areas overlap or not.
; Each direction uses a block of 64 LDI or LDD instructions. The code jumps
; into the block so that the first pass copies (count mod 64) bytes. Every
; later pass copies a full 64 bytes. LDI and LDD set P/V when BC is not 0,
; so JP PE repeats the block until BC reaches 0.

memmove:
        ld      a,b
        or      c
        ret     z               ; count = 0: nothing to do

        ; carry set if dst < src
        ld      a,e
        sub     l
        ld      a,d
        sbc     a,h
        jp      c,memmove_fwd

        ; backward copy: dst >= src
        add     hl,bc
        dec     hl              ; HL = src + count - 1
        ex      de,hl
        add     hl,bc
        dec     hl              ; HL = dst + count - 1
        ex      de,hl

        ld      a,c
        neg
        and     63
        add     a,a             ; entry offset = 2*((-count) & 63)
        exx
        ld      e,a
        ld      d,0
        ld      ix,memmove_bblk
        add     ix,de
        exx
        jp      (ix)
memmove_bblk:
        DUP 64
        ldd
        EDUP
        jp      pe,memmove_bblk
        ret

memmove_fwd:   ; forward copy: dst < src
        ld      a,c
        neg
        and     63
        add     a,a             ; entry offset = 2*((-count) & 63)
        exx
        ld      e,a
        ld      d,0
        ld      ix,memmove_fblk
        add     ix,de
        exx
        jp      (ix)
memmove_fblk:
        DUP 64
        ldi
        EDUP
        jp      pe,memmove_fblk
        ret
```

Worst-case cycles: 1058987