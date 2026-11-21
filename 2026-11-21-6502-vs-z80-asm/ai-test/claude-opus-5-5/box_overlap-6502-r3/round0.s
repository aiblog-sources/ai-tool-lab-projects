; box_overlap
; Box A at $00-$03 (x, y, w, h). Box B at $04-$07 (x, y, w, h).
; Returns with carry set if the boxes overlap, carry clear if they do not.
;
; Each axis is tested the same way:
;   d = ax - bx
;   If ax >= bx: overlap needs d < bw and aw != 0.
;   If ax <  bx: A holds 256 - (bx - ax) and carry is clear.
;                aw + A > 256 means bx - ax < aw.
;                In that case the add sets carry and leaves A nonzero.
;                It also needs bw != 0.
; Every sum is effectively 9 bits wide, so 8-bit wraparound cannot happen.

box_overlap:
        ; ---- X axis ----
        lda $00
        sec
        sbc $04             ; A = ax - bx
        bcc @xlt            ; ax < bx
        cmp $06             ; d >= bw ?
        bcs @fail
        lda $02             ; aw == 0 ?
        bne @y
@fail:
        clc
        rts
@xlt:
        adc $02             ; carry is clear here: A = 256 - (bx-ax) + aw
        bcc @fail           ; aw < bx-ax
        beq @fail           ; aw == bx-ax
        lda $06             ; bw == 0 ?
        beq @fail

        ; ---- Y axis ----
@y:
        lda $01
        sec
        sbc $05             ; A = ay - by
        bcc @ylt            ; ay < by
        cmp $07             ; d >= bh ?
        bcs @fail2
        lda $03             ; ah == 0 ?
        bne @ok
@fail2:
        clc
        rts
@ylt:
        adc $03             ; carry is clear here: A = 256 - (by-ay) + ah
        bcc @fail2
        beq @fail2
        lda $07             ; bh == 0 ?
        beq @fail2
@ok:
        sec
        rts
