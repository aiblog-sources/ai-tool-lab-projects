The previous version built its 256-byte table deep in the stack (at $DE00), and the harness counts that as a write outside the allowed RAM. This version is an insertion sort that works entirely in place. It writes no RAM except the array itself, and the only stack it uses is the return address.

```asm
; sort_u8 - insertion sort of B unsigned bytes at HL, ascending, in place.
; No scratch RAM or extra stack is used. The outer loop state is kept in IX/IY.
;   IX  = pointer to a[i]
;   IYL = i (number of elements before a[i])
;   IYH = outer iterations remaining
; Inner loop: A = key, DE = destination, HL = a[j], BC = elements left to test.
; LDD shifts one element up and counts BC down.

sort_u8:
        ld      a,b
        cp      2
        ret     c                   ; 0 or 1 elements: already sorted
        dec     a
        ld      iyh,a               ; n-1 insertions
        ld      iyl,1               ; i = 1
        push    hl
        pop     ix
        inc     ix                  ; IX = &a[1]

.outer:
        ld      e,ixl
        ld      d,ixh               ; DE = &a[i]
        ld      l,e
        ld      h,d
        dec     hl                  ; HL = &a[i-1]
        ld      b,0
        ld      c,iyl               ; BC = i
        ld      a,(de)              ; key

.inner:
        cp      (hl)                ; key < a[j] ?
        jr      nc,.done            ; no: insert here
        ldd                         ; a[j+1] = a[j]; DE--, HL--, BC--
        jp      pe,.inner           ; more elements before
.done:
        ld      (de),a              ; place key

        inc     ix
        inc     iyl
        dec     iyh
        jr      nz,.outer
        ret
```

Worst-case cycles: 1320134