; box_overlap
; In:  HL -> ax, ay, aw, ah, bx, by, bw, bh (unsigned bytes)
; Out: carry set if the boxes overlap, clear if not.
;
; Per axis, with d = a - b (pos - pos):
;   if a >= b : overlap  <=>  d < bw          (b < a + aw holds since aw >= 1)
;   if a <  b : overlap  <=>  (b - a) < aw  and bw != 0
; aw and ah are checked for zero up front.
; On the d >= 0 path, d < bw already fails when bw = 0.
; On the d < 0 path, bw (or bh) is checked explicitly.

box_overlap:
        ld      b,(hl)          ; B = ax
        inc     hl
        ld      c,(hl)          ; C = ay
        inc     hl
        ld      a,(hl)          ; aw
        and     a               ; clears carry
        ret     z               ; aw = 0 -> no overlap
        ld      d,a             ; D = aw
        inc     hl
        ld      a,(hl)          ; ah
        and     a
        ret     z               ; ah = 0 -> no overlap
        ld      e,a             ; E = ah
        inc     hl              ; HL -> bx

        ; ---- X axis ----
        ld      a,b
        sub     (hl)            ; ax - bx
        inc     hl
        inc     hl              ; HL -> bw
        jr      c,.xneg
        cp      (hl)            ; (ax-bx) < bw ?
        ret     nc              ; no -> carry clear, no overlap
.y:
        dec     hl              ; HL -> by
        ld      a,c
        sub     (hl)            ; ay - by
        inc     hl
        inc     hl              ; HL -> bh
        jr      c,.yneg
        cp      (hl)            ; carry set iff (ay-by) < bh
        ret

.xneg:
        neg                     ; bx - ax (1..255)
        cp      d               ; < aw ?
        ret     nc
        ld      a,(hl)          ; bw
        and     a               ; carry cleared
        jr      nz,.y
        ret                     ; bw = 0 -> no overlap

.yneg:
        neg                     ; by - ay
        cp      e               ; < ah ?
        ret     nc
        ld      a,(hl)          ; bh
        and     a
        ret     z               ; bh = 0 -> no overlap (carry clear)
        scf
        ret
