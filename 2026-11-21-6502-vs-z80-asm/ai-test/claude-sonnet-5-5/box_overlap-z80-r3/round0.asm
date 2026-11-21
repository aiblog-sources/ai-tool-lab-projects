box_overlap:
        push hl
        call box_overlap_axis   ; x axis
        pop hl                  ; pop leaves flags alone
        ret nc
        inc hl                  ; point at y
        jp box_overlap_axis     ; y axis, its RET returns to our caller

; HL -> pos of A (offsets: +0 apos, +2 alen, +4 bpos, +6 blen)
; Returns carry set if the intervals overlap, and zero length never overlaps.
box_overlap_axis:
        ld a,(hl)               ; apos
        inc hl
        inc hl
        ld c,(hl)               ; alen
        inc hl
        inc hl
        ld d,(hl)               ; bpos
        inc hl
        inc hl
        ld e,(hl)               ; blen
        ld b,a
        ld a,c
        or a
        jr z,.fail
        ld a,e
        or a
        jr z,.fail
        ld a,b
        sub d                   ; apos - bpos
        jr c,.c1ok              ; apos < bpos, and blen > 0, so apos < bpos+blen
        cp e                    ; (apos-bpos) < blen ?
        jr nc,.fail
.c1ok:
        ld a,d
        sub b                   ; bpos - apos
        jr c,.ok                ; bpos < apos, and alen > 0
        cp c                    ; (bpos-apos) < alen ? carry = result
        ret
.ok:
        scf
        ret
.fail:
        or a                    ; clear carry
        ret
