; box_overlap
; Box A: $00=x $01=y $02=w $03=h
; Box B: $04=x $05=y $06=w $07=h
; Returns C=1 if the boxes overlap, C=0 if they do not.
;
; Per axis, d = a - b (8-bit, wrapping):
;   a >  b : overlap iff d < bw          (b < a + aw always holds)
;   a == b : overlap iff aw != 0 and bw != 0
;   a <  b : overlap iff aw > (b - a), i.e. d + aw >= 257
;            (carry out and nonzero result); a < b + bw always holds

box_overlap:
        ; ---- X axis ----
        lda $00
        sec
        sbc $04             ; ax - bx
        bcc @xb             ; ax < bx
        beq @xeq            ; ax == bx
        cmp $06             ; (ax - bx) < bw ?
        bcc @y
@fail:
        clc
        rts
@xeq:
        lda $02
        beq @fail
        lda $06
        beq @fail
        bne @y
@xb:                        ; carry is clear here
        adc $02             ; (256 - k) + aw
        bcc @fail           ; aw < k
        beq @fail           ; aw == k
        ; ---- Y axis ----
@y:
        lda $01
        sec
        sbc $05             ; ay - by
        bcc @yb             ; ay < by
        beq @yeq            ; ay == by
        cmp $07             ; (ay - by) < bh ?
        bcs @fail
        sec
        rts
@yeq:
        lda $03
        beq @fail
        lda $07
        beq @fail
        sec
        rts
@yb:                        ; carry is clear here
        adc $03             ; (256 - k) + ah
        bcc @fail
        beq @fail
        rts                 ; carry is set from the adc
