```asm
; sort_u8: insertion sort, unsigned bytes, ascending, in place
; In:  ($00) = array pointer, X = length (0..255)
; Scratch: $02 = length, $03 = outer index i, $04 = key
sort_u8:
        stx $02
        cpx #2
        bcc sort_u8_done        ; length 0 or 1: nothing to do
        ldy #1                  ; i = 1
sort_u8_outer:
        lda ($00),y
        sta $04                 ; key = a[i]
        sty $03
sort_u8_inner:
        dey                     ; Y = j-1
        lda ($00),y             ; a[j-1]
        cmp $04
        bcc sort_u8_found       ; a[j-1] < key: insert at j
        iny
        sta ($00),y             ; a[j] = a[j-1]
        dey
        bne sort_u8_inner
        beq sort_u8_place       ; j reached 0 (always taken)
sort_u8_found:
        iny
sort_u8_place:
        lda $04
        sta ($00),y             ; a[j] = key
        ldy $03
        iny
        cpy $02
        bcc sort_u8_outer
sort_u8_done:
        rts
```
Worst-case cycles: 850666