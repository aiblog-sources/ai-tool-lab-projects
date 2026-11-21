```asm
; sort_u8: counting sort, unsigned bytes, ascending, in place.
; In:  HL = array pointer, B = length (0..255)
; Uses: a 256-byte count table on the stack, aligned to a 256-byte page.
;       The table takes at most 511 bytes below the entry SP.
;       $C000-$C001 hold the saved SP, $C002-$C003 hold the saved pointer.

sort_u8:
        ld a,b
        or a
        ret z                   ; length 0: nothing to do

        ld (#C000),sp           ; save the original SP
        ld (#C002),hl           ; save the array pointer
        ex de,hl                ; DE = array pointer

        ; Move SP down to a page boundary. The table is the page below it.
        ld hl,0
        add hl,sp
        ld l,0
        ld sp,hl                ; SP = (SP & $FF00)
        ld a,h
        dec a                   ; A = table page

        ; Clear 256 bytes: (A*256) .. (A*256+255)
        ld hl,0
        DUP 128
        push hl
        EDUP

        ld h,a                  ; H = table page

        ; Count each value
.count:
        ld a,(de)
        ld l,a
        inc (hl)
        inc de
        djnz .count

        ; Write the values back in ascending order
        ld de,(#C002)           ; DE = output pointer
        ld l,0
.scan:
        ld a,(hl)
        or a
        jr nz,.emit
        inc l
        jp nz,.scan
        jr .done

.emit:
        ld b,a                  ; B = count of this value
        ld a,l                  ; A = value
.wr:
        ld (de),a
        inc de
        djnz .wr
        inc l
        jp nz,.scan

.done:
        ld sp,(#C000)           ; restore the original SP
        ret
```

Worst-case cycles: 28915