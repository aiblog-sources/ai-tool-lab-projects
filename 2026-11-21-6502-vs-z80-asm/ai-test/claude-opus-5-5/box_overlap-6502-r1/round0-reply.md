```asm
; box_overlap: AABB overlap test
; Box A: $00=x $01=y $02=w $03=h
; Box B: $04=x $05=y $06=w $07=h
; Output: C=1 if the boxes overlap, C=0 if not.
;
; Per axis, with d = a - b:
;   a >= b : overlap iff (a - b) < bsize and asize != 0
;   a <  b : overlap iff (b - a) < asize and bsize != 0
; (b - a) < asize is tested as asize + (a - b mod 256): this
; gives C=1 and a nonzero result exactly when asize > b - a.
; A size of 0 always fails the test.

box_overlap:
        ; ---- X axis ----
        lda $00
        sec
        sbc $04                 ; A = ax - bx
        bcc @xneg               ; ax < bx
        cmp $06                 ; (ax - bx) < bw ?
        bcs @fail
        lda $02                 ; aw must be nonzero
        beq @fail
        bne @y                  ; always taken
@xneg:
        adc $02                 ; C clear here: aw - (bx - ax)
        bcc @fail               ; aw < bx - ax
        beq @fail               ; aw == bx - ax
        lda $06                 ; bw must be nonzero
        beq @fail
@y:
        ; ---- Y axis ----
        lda $01
        sec
        sbc $05                 ; A = ay - by
        bcc @yneg               ; ay < by
        cmp $07                 ; (ay - by) < bh ?
        bcs @fail
        lda $03
        cmp #1                  ; C = (ah != 0)
        rts
@yneg:
        adc $03                 ; C clear here: ah - (by - ay)
        bcc @fail
        beq @fail
        lda $07
        cmp #1                  ; C = (bh != 0)
        rts
@fail:
        clc
        rts
```

Worst-case cycles: 58