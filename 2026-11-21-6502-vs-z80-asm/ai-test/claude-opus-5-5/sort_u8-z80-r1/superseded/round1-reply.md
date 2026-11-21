The previous version put a 256-byte count table on the stack, and that wrote RAM outside the allowed area. This version is an in-place insertion sort. It keeps its state in the $C000-$C004 scratch area, and its only stack use is the CALL's return address. The inner shift loop uses `LDD`: it moves one byte up, steps both pointers down, counts down BC, and sets P/V while elements remain.

```asm
; sort_u8: insertion sort, ascending, in place
; In:  HL = pointer to array, B = length (0..255)
; Scratch: $C000-$C004 only; no stack use beyond the CALL itself.

sort_u8_ptr     equ $C000          ; word: pointer to a[i]
sort_u8_i       equ $C002          ; word: i (high byte stays 0)
sort_u8_rem     equ $C004          ; byte: outer iterations left

sort_u8:
        ld a,b
        cp 2
        ret c                   ; length 0 or 1: already sorted
        dec a
        ld (sort_u8_rem),a      ; n-1 outer passes
        inc hl
        ld (sort_u8_ptr),hl     ; ptr = &a[1]
        ld hl,1
        ld (sort_u8_i),hl       ; i = 1

.outer:
        ld hl,(sort_u8_ptr)
        ld d,h
        ld e,l                  ; DE = &a[i]
        inc hl
        ld (sort_u8_ptr),hl     ; advance for next pass
        ld bc,(sort_u8_i)       ; BC = i (elements before a[i])
        ld h,d
        ld l,e
        dec hl                  ; HL = &a[i-1]
        ld a,(de)               ; A = key
.inner:
        cp (hl)                 ; carry if key < a[j-1]
        jr nc,.ins
        ldd                     ; a[j] = a[j-1]; DE--, HL--, BC--
        jp pe,.inner            ; continue while BC != 0
.ins:
        ld (de),a               ; place key
        ld hl,sort_u8_i
        inc (hl)
        ld hl,sort_u8_rem
        dec (hl)
        jr nz,.outer
        ret
```

Worst-case cycles: 1333095