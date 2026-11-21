```asm
; sort_u8: in-place ascending sort of unsigned bytes
; In: HL = array pointer, B = length (0..255)
; Bubble sort, fixed n-1 passes of n-1 compares, no early exit.
; Uses only the stack for temporaries; no scratch RAM needed.
sort_u8:
        ld   a,b
        cp   2
        ret  c              ; length 0 or 1: nothing to do
        dec  b              ; B = n-1 passes
.outer:
        push bc             ; save pass counter
        push hl             ; save array start
.inner:
        ld   a,(hl)
        inc  hl
        cp   (hl)
        jr   c,.noswap      ; a < next: in order (equal swap is harmless)
        ld   c,(hl)
        ld   (hl),a
        dec  hl
        ld   (hl),c
        inc  hl
.noswap:
        djnz .inner         ; B = n-1 compares per pass
        pop  hl
        pop  bc
        djnz .outer
        ret
```
Worst-case cycles: 4722410