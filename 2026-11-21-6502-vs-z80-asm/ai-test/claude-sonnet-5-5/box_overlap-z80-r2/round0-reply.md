```asm
; box_overlap
; In:  HL -> 8 bytes: A(x,y,w,h), B(x,y,w,h)
; Out: carry set if the boxes overlap, clear if not
; Trashes: A, B, C, D, E, HL, flags
box_overlap:
        push hl
        call box_overlap_axis   ; X axis (offsets 0,2,4,6)
        pop hl
        ret nc                  ; no overlap on X -> carry clear
        inc hl                  ; Y axis (offsets 1,3,5,7)
        jp box_overlap_axis     ; its RET returns to our caller

; HL -> ax; size of A at +2, bx at +4, size of B at +6
; Returns carry set if the 1-D intervals overlap
box_overlap_axis:
        ld e,(hl)               ; ax
        inc hl
        inc hl
        ld c,(hl)               ; aw
        inc hl
        inc hl
        ld b,(hl)               ; bx
        inc hl
        inc hl
        ld d,(hl)               ; bw
        ld a,c
        or a
        ret z                   ; aw = 0 -> no overlap (carry clear)
        ld a,d
        or a
        ret z                   ; bw = 0 -> no overlap (carry clear)
        ld a,e
        sub b                   ; ax - bx
        jr c,.a_left
        cp d                    ; ax >= bx: overlap if ax-bx < bw
        ret
.a_left:
        neg                     ; bx - ax (nonzero)
        cp c                    ; overlap if bx-ax < aw
        ret
```
Worst-case cycles: 340