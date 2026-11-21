The previous version built its 256-byte count table on the stack, which wrote below the allowed area (the $DE00 page). This version is an in-place insertion sort. It uses only $C000-$C004 as scratch and the 2 bytes the CALL itself pushes.

```asm
; sort_u8: insertion sort, unsigned bytes, ascending, in place.
; In:  HL = array pointer, B = length (0..255)
; Scratch: $C000-$C001 = pointer to a[i]
;          $C002-$C003 = i (16-bit)
;          $C004       = n
; No stack use beyond the CALL/RET itself.

sort_u8:
        ld a,b
        cp 2
        ret c                   ; length 0 or 1: already sorted

        ld (#C004),a            ; n
        inc hl
        ld (#C000),hl           ; p = &a[1]
        ld hl,1
        ld (#C002),hl           ; i = 1

.outer:
        ld hl,(#C000)           ; HL = &a[i]
        ld a,(hl)               ; A = key
        ld d,h
        ld e,l                  ; DE = hole = &a[i]
        dec hl                  ; HL = &a[i-1]
        ld bc,(#C002)           ; BC = i (max shifts)

.inner:
        cp (hl)                 ; key - a[j]
        jr nc,.place            ; key >= a[j]: stop
        ldd                     ; a[j+1] = a[j]; HL--, DE--, BC--
        jp pe,.inner            ; continue while BC != 0

.place:
        ld (de),a               ; drop key into the hole

        ld hl,(#C000)
        inc hl
        ld (#C000),hl           ; p++
        ld hl,(#C002)
        inc hl
        ld (#C002),hl           ; i++
        ld a,(#C004)
        cp l
        jr nz,.outer            ; loop while i != n

        ret
```

Worst-case cycles: 1338425