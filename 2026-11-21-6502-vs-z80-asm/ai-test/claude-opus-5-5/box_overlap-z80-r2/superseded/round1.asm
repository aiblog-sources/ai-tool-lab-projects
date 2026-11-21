; box_overlap: axis-aligned box overlap test
; In:  HL -> ax, ay, aw, ah, bx, by, bw, bh
; Out: carry set if overlap, clear if not

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
        inc     hl
        inc     hl              ; -> bw
        jr      c,.xneg
        jr      z,.xeq
        cp      (hl)            ; (ax-bx) < bw ?
        ret     nc
        jr      .y
.xeq:
        cp      (hl)            ; 0 < bw ?
        ret     nc
        ld      a,d
        or      a               ; aw == 0 -> no overlap (carry clear)
        ret     z
        jr      .y
.xneg:
        neg                     ; bx - ax
        cp      d               ; < aw ?
        ret     nc

        ; ---- Y axis ----
.y:
        dec     hl              ; -> by
        ld      a,c
        sub     (hl)            ; ay - by
        inc     hl
        inc     hl              ; -> bh
        jr      c,.yneg
        jr      z,.yeq
        cp      (hl)            ; (ay-by) < bh ?
        ret
.yeq:
        cp      (hl)            ; 0 < bh ?
        ret     nc
        ld      a,e
        or      a               ; ah == 0 -> no overlap
        ret     z
        scf
        ret
.yneg:
        neg                     ; by - ay
        cp      e               ; < ah ?
        ret
