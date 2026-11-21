; box_overlap
; In:  HL -> ax, ay, aw, ah, bx, by, bw, bh (8 unsigned bytes)
; Out: carry set if the boxes overlap, clear if not.
;
; For each axis, take d = b - a.
;   b >= a : overlap on this axis iff d < aw and bw != 0
;   b <  a : overlap on this axis iff a - b < bw and aw != 0
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
        ld      a,(hl)          ; bw must be nonzero
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
        xor     a
        cp      (hl)            ; carry if bh != 0
        ret
.yneg:
        dec     a
        add     a,(hl)          ; carry if ay - by < bh
        ret     nc
        xor     a
        cp      d               ; carry if ah != 0
        ret

.xneg:
        dec     a
        add     a,(hl)          ; carry if ax - bx < bw
        ret     nc
        ld      a,e             ; aw must be nonzero
        or      a
        ret     z               ; carry is clear here
        ; Y axis, a copy of .y so this path needs no jump
        dec     hl              ; HL -> by
        ld      a,(hl)
        inc     hl
        inc     hl              ; HL -> bh
        sub     b
        jp      c,.yneg2
        cp      d
        ret     nc
        xor     a
        cp      (hl)
        ret
.yneg2:
        dec     a
        add     a,(hl)
        ret     nc
        xor     a
        cp      d
        ret
