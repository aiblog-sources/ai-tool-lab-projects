```asm
sort_u8:
        ld   a,b
        cp   2
        ret  c                  ; fewer than 2 elements: nothing to do
        dec  b                  ; B = number of insertions (n-1)
        ld   d,1                ; D = i (size of sorted prefix)
        inc  hl                 ; HL = &a[1]
.outer:
        ld   a,(hl)             ; key = a[i]
        push hl                 ; remember &a[i]
        ld   c,d                ; C = remaining slots to the left
.inner:
        dec  hl
        cp   (hl)               ; key - a[j-1]
        jr   nc,.put1           ; key >= a[j-1]: insert at j
        ld   e,(hl)             ; shift a[j-1] up to a[j]
        inc  hl
        ld   (hl),e
        dec  hl
        dec  c
        jr   nz,.inner
        ld   (hl),a             ; reached the start: store key at a[0]
.next:
        pop  hl
        inc  hl
        inc  d
        djnz .outer
        ret
.put1:
        inc  hl
        ld   (hl),a
        jr   .next
```
Worst-case cycles: 2022403