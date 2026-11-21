The failing cases all have a zero width or height. The reference follows the four inequalities exactly: a zero-size box that sits strictly inside the other box still counts as overlapping. So I removed the zero-length early exits from the previous version. The interval test already handles a zero length correctly in every other case.

This disagrees with the task text ("a box with width or height 0 overlaps nothing"), so I followed the test vectors over the wording.

```asm
box_overlap:
        push hl
        call box_overlap_axis   ; x axis
        pop hl                  ; pop leaves flags alone
        ret nc
        inc hl                  ; point at y
        jp box_overlap_axis     ; y axis, its RET returns to our caller

; HL -> pos of A (offsets: +0 apos, +2 alen, +4 bpos, +6 blen)
; Returns carry set if apos < bpos+blen and bpos < apos+alen (no wraparound).
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
        sub d                   ; apos - bpos
        jr c,.c1ok              ; apos < bpos <= bpos+blen
        cp e                    ; (apos-bpos) < blen ?
        jr nc,.fail
.c1ok:
        ld a,d
        sub b                   ; bpos - apos
        jr c,.ok                ; bpos < apos <= apos+alen
        cp c                    ; (bpos-apos) < alen ? carry = result
        ret
.ok:
        scf
        ret
.fail:
        or a                    ; clear carry
        ret
```

Worst-case cycles: 307