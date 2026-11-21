; box_overlap
; In:  HL -> ax, ay, aw, ah, bx, by, bw, bh (8 unsigned bytes)
; Out: carry set if the boxes overlap, clear if not.
;
; Each axis is tested with the exact formula, using d = b - a:
;   b >  a : a < b + bw always holds, so overlap iff d < aw
;   b == a : overlap iff aw != 0 and bw != 0
;            (d < aw already gives aw != 0, so only bw is checked)
;   b <  a : b < a + aw always holds, so overlap iff a - b < bw.
;            d holds b - a + 256 (1..255), so a - b < bw is the same as
;            (d - 1) + bw >= 256, which is the carry out of the ADD.
box_overlap:
        ld      c,(hl)          ; C = ax
        inc     hl
        ld      b,(hl)          ; B = ay
        inc     hl
        ld      e,(hl)          ; E = aw
        inc     hl
        ld      d,(hl)          ; D = ah
        inc     hl
        ld      a,(hl)          ; A = bx
        inc     hl
        inc     hl              ; HL -> bw
        sub     c               ; A = bx - ax, carry if bx < ax
        jp      c,.xneg
        ; bx >= ax
        cp      e               ; carry if (bx - ax) < aw
        ret     nc
        or      a
        jp      nz,.y           ; bx > ax: X axis overlaps
        ld      a,(hl)          ; bx == ax: bw must be nonzero
        or      a
        ret     z               ; carry is clear here
.y:
        dec     hl              ; HL -> by
        ld      a,(hl)
        inc     hl
        inc     hl              ; HL -> bh
        sub     b               ; A = by - ay, carry if by < ay
        jp      c,.yneg
        ; by >= ay
        cp      d               ; carry if (by - ay) < ah
        ret     nc
        or      a
        scf
        ret     nz              ; by > ay: overlap
        cp      (hl)            ; A = 0: carry if bh != 0
        ret
.yneg:
        dec     a
        add     a,(hl)          ; carry if ay - by < bh
        ret

.xneg:
        dec     a
        add     a,(hl)          ; carry if ax - bx < bw
        ret     nc
        jp      .y
