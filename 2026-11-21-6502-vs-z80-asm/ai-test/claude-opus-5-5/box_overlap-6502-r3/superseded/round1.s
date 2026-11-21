; box_overlap
; Box A at $00-$03 (x, y, w, h). Box B at $04-$07 (x, y, w, h).
; Returns with carry set if the boxes overlap, carry clear if they do not.
; Each axis tests a < b + bs and b < a + as with 9-bit sums.

box_overlap:
        ; ---- X axis ----
        lda $00
        sec
        sbc $04             ; A = ax - bx
        bcc @xlt            ; ax < bx
        beq @xeq            ; ax == bx
        cmp $06             ; ax > bx: need d < bw
        bcc @y
@fail:
        clc
        rts
@xeq:
        lda $06             ; need bw > 0
        beq @fail
        lda $02             ; need aw > 0
        beq @fail
        bne @y
@xlt:
        adc $02             ; carry clear: A = 256 - (bx-ax) + aw
        bcc @fail           ; aw < bx-ax
        beq @fail           ; aw == bx-ax

        ; ---- Y axis ----
@y:
        lda $01
        sec
        sbc $05             ; A = ay - by
        bcc @ylt            ; ay < by
        beq @yeq            ; ay == by
        cmp $07             ; ay > by: need d < bh
        bcc @ok
@fail2:
        clc
        rts
@yeq:
        lda $07             ; need bh > 0
        beq @fail2
        lda $03             ; need ah > 0
        beq @fail2
        bne @ok
@ylt:
        adc $03             ; carry clear: A = 256 - (by-ay) + ah
        bcc @fail2
        beq @fail2
@ok:
        sec
        rts
