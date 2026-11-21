; box_overlap: axis-aligned box overlap test
; In:  HL -> ax, ay, aw, ah, bx, by, bw, bh
; Out: carry set if the boxes overlap, carry clear if not
;
; For each axis, let d = a - b.
;   If a >= b, the test b < a + aw always passes when aw > 0.
;     So overlap needs d < bw and aw != 0.
;     If bw = 0, then d < 0 is impossible, so bw = 0 fails on its own.
;   If a < b, the test a < b + bw always passes when bw > 0.
;     So overlap needs (b - a) < aw and bw != 0.
;     If aw = 0, the compare fails on its own.
; Working with differences avoids any 9-bit sums.

box_overlap:
        ld      b,(hl)          ; ax
        inc     hl
        ld      c,(hl)          ; ay
        inc     hl
        ld      d,(hl)          ; aw
        inc     hl
        ld      e,(hl)          ; ah
        inc     hl              ; -> bx

        ; ---- X axis ----
        ld      a,b
        sub     (hl)            ; ax - bx
        inc     hl              ; -> by
        inc     hl              ; -> bw
        jr      c,.xneg
        cp      (hl)            ; (ax-bx) < bw ?
        ret     nc
        ld      a,d
        or      a               ; aw == 0 ? (clears carry)
        ret     z
        jr      .y
.xneg:
        neg                     ; bx - ax (1..255)
        cp      d               ; < aw ?
        ret     nc
        ld      a,(hl)
        or      a               ; bw == 0 ? (clears carry)
        ret     z

        ; ---- Y axis ----
.y:
        dec     hl              ; -> by
        ld      a,c
        sub     (hl)            ; ay - by
        inc     hl              ; -> bw
        inc     hl              ; -> bh
        jr      c,.yneg
        cp      (hl)            ; (ay-by) < bh ?
        ret     nc
        ld      a,e
        or      a               ; ah == 0 ?
        ret     z
        scf
        ret
.yneg:
        neg                     ; by - ay
        cp      e               ; < ah ?
        ret     nc
        ld      a,(hl)
        or      a               ; bh == 0 ?
        ret     z
        scf
        ret
