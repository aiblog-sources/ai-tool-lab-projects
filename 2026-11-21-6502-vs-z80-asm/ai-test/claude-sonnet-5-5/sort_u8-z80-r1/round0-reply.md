```asm
; sort_u8: insertion sort, ascending, unsigned bytes, in place.
; In:  HL = pointer to array, B = length (0..255)
; Out: array sorted in place
; Uses the stack (2 bytes) for the outer counter; no RAM scratch.
sort_u8:
        ld   a,b
        cp   2
        ret  c                  ; length 0 or 1: nothing to do
        ld   d,h
        ld   e,l
        inc  de                 ; DE = &a[1]
        dec  b                  ; B = n-1 outer passes
        ld   c,1                ; C = i (elements before DE)
.outer:
        push bc
        ld   a,(de)             ; key = a[i]
        ld   b,c                ; B = inner count = i
        ld   c,a                ; C = key
        ld   h,d
        ld   l,e                ; HL = &a[i]
.inner:
        dec  hl
        ld   a,c
        cp   (hl)               ; key - a[j]
        jr   nc,.place_after    ; a[j] <= key: stop
        ld   a,(hl)
        inc  hl
        ld   (hl),a             ; a[j+1] = a[j]
        dec  hl
        djnz .inner
        ld   (hl),c             ; reached the start: key goes at a[0]
        jr   .next
.place_after:
        inc  hl
        ld   (hl),c
.next:
        pop  bc
        inc  de
        inc  c
        djnz .outer
        ret
```
Worst-case cycles: 2060892