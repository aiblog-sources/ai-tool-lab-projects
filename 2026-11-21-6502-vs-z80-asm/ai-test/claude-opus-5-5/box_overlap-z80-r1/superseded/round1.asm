; box_overlap
; In:  HL -> ax, ay, aw, ah, bx, by, bw, bh (unsigned bytes)
; Out: carry set if
;        ax < bx+bw  and  bx < ax+aw  and  ay < by+bh  and  by < ay+ah
;      (sums computed in 9 bits), carry clear otherwise.

box_overlap:
        ld      b,(hl)          ; B = ax
        inc     hl
        ld      c,(hl)          ; C = ay
        inc     hl
        ld      d,(hl)          ; D = aw
        inc     hl
        ld      e,(hl)          ; E = ah
        inc     hl              ; HL -> bx

        ; ---- bx < ax + aw ----
        ld      a,b
        add     a,d             ; ax + aw
        jr      c,.x2ok         ; sum >= 256 > bx
        ld      d,a
        ld      a,(hl)          ; bx
        cp      d               ; carry iff bx < ax+aw
        ret     nc
.x2ok:
        ; ---- ax < bx + bw ----
        ld      a,(hl)          ; bx
        inc     hl
        inc     hl              ; HL -> bw
        add     a,(hl)          ; bx + bw
        jr      c,.x1ok
        ld      d,a
        ld      a,b             ; ax
        cp      d               ; carry iff ax < bx+bw
        ret     nc
.x1ok:
        dec     hl              ; HL -> by

        ; ---- by < ay + ah ----
        ld      a,c
        add     a,e             ; ay + ah
        jr      c,.y2ok
        ld      e,a
        ld      a,(hl)          ; by
        cp      e               ; carry iff by < ay+ah
        ret     nc
.y2ok:
        ; ---- ay < by + bh ----
        ld      a,(hl)          ; by
        inc     hl
        inc     hl              ; HL -> bh
        add     a,(hl)          ; by + bh
        ret     c               ; sum >= 256 > ay -> overlap (carry set)
        ld      e,a
        ld      a,c             ; ay
        cp      e               ; carry iff ay < by+bh
        ret
