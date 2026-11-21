The failing cases all have a zero width or height, and the expected result is 1. In each one the four inequalities are satisfied as written (for example 5<20, 0<5, 0<12, 4<8). So the checker uses the plain inequalities, and a zero-size box inside the other box counts as an overlap. My zero-size early exits were wrong, so I removed them. The rest of the routine is unchanged.

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

Worst-case cycles: 363