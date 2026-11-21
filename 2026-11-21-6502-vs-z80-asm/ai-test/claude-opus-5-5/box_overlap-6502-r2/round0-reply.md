```asm
; box_overlap
; Box A: $00=x $01=y $02=w $03=h
; Box B: $04=x $05=y $06=w $07=h
; Returns C=1 if the boxes overlap, C=0 if they do not.
;
; Per axis, d = a - b is computed with 8-bit wraparound:
;   a >= b (no borrow): overlap needs d < bw and aw != 0
;   a <  b (borrow):    d = 256 - k, where k = b - a (1..255).
;                       Overlap needs aw > k, which is the same as
;                       d + aw >= 257 (carry out and a nonzero result).
;                       It also needs bw != 0.
; Zero sizes are rejected by these tests, and no 9-bit sum is needed.

box_overlap:
        ; ---- X axis ----
        lda $00
        sec
        sbc $04             ; ax - bx
        bcc @xb             ; ax < bx
        cmp $06             ; (ax - bx) < bw ?
        bcs @fail
        lda $02             ; aw must be nonzero
        bne @y
@fail:
        clc
        rts
@xb:                        ; carry is clear here
        adc $02             ; (256 - k) + aw
        bcc @fail           ; aw <= k - 1
        beq @fail           ; aw == k
        lda $06             ; bw must be nonzero
        beq @fail
        ; ---- Y axis ----
@y:
        lda $01
        sec
        sbc $05             ; ay - by
        bcc @yb             ; ay < by
        cmp $07             ; (ay - by) < bh ?
        bcs @fail
        lda $03             ; ah must be nonzero
        beq @fail
        sec
        rts
@yb:                        ; carry is clear here
        adc $03             ; (256 - k) + ah
        bcc @fail
        beq @fail
        lda $07             ; bh must be nonzero
        beq @fail
        rts                 ; carry is still set from the adc
```

Worst-case cycles: 58