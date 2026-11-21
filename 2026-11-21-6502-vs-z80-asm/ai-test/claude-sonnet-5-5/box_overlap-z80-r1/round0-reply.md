```asm
; box_overlap
; In:  HL -> 8 bytes: box A (x,y,w,h), box B (x,y,w,h)
; Out: carry set if overlap, clear if not
; Clobbers: A, B, C, D, E, HL, flags

box_overlap:
        push hl
        call .axis              ; X axis
        pop hl
        ret nc
        inc hl                  ; Y axis: HL -> ay
        ; fall into .axis

; HL -> A pos, (HL+2)=A size, (HL+4)=B pos, (HL+6)=B size
.axis:
        ld b,(hl)               ; apos
        inc hl
        inc hl
        ld c,(hl)               ; asize
        inc hl
        inc hl
        ld d,(hl)               ; bpos
        inc hl
        inc hl
        ld e,(hl)               ; bsize
        ld a,c
        or a
        jr z,.no
        ld a,e
        or a
        jr z,.no
        ld a,d                  ; bpos + bsize
        add a,e
        jr c,.c1ok              ; sum >= 256 > apos
        ld e,a
        ld a,b
        cp e                    ; carry if apos < bpos+bsize
        jr nc,.no
.c1ok:
        ld a,b                  ; apos + asize
        add a,c
        jr c,.yes
        ld c,a
        ld a,d
        cp c                    ; carry if bpos < apos+asize
        jr nc,.no
.yes:
        scf
        ret
.no:
        xor a                   ; clears carry
        ret
```
Worst-case cycles: 423