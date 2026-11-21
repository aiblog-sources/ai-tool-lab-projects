```asm
; sort_u8: counting sort, ascending, in place
; In:  HL = pointer to array, B = length (0..255)
; Uses a 256-byte count table on the stack, released before returning.
sort_u8:
        ld a,b
        or a
        ret z                   ; length 0: nothing to do

        ex de,hl                ; DE = array pointer
        push de
        pop iy                  ; IY = copy of the array start
        ld hl,0
        ld c,b                  ; C = saved length
        ld b,16
.clr:                           ; push 128 zero words = 256 bytes of counts
        push hl
        push hl
        push hl
        push hl
        push hl
        push hl
        push hl
        push hl
        djnz .clr               ; the count table starts at SP

        ld b,c                  ; B = length
        ld c,l                  ; C = 0
.cnt:                           ; count[value]++ for each element
        ld a,(de)
        inc de
        ld l,a
        ld h,c
        add hl,sp
        inc (hl)
        djnz .cnt

        push iy
        pop de                  ; DE = array start again
        ld hl,0
        add hl,sp               ; HL = &count[0]
        xor a                   ; A = current value 0
.outer:
        ld b,(hl)
        inc hl
        inc b
        dec b
        jr z,.skip
.inner:                         ; write the value count times
        ld (de),a
        inc de
        djnz .inner
.skip:
        inc a
        jr nz,.outer            ; values 0..255

        ld sp,hl                ; HL = table base + 256, the original SP
        ret
```
Worst-case cycles: 35199