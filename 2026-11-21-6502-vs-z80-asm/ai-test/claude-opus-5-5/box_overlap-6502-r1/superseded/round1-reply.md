Your test vectors follow the four inequalities literally, so a zero-size box can still overlap. In your first example, a = [5, 0, 0, 8] and b = [0, 4, 20, 8], every inequality holds: 5 < 20, 0 < 5, 0 < 12 and 4 < 8. My previous version wrongly rejected any box with a zero size. The fix applies the formula exactly on each axis, with d = a − b:

- **a < b:** a < b + bsize is automatically true. Only b − a < asize needs testing.
- **a > b:** b < a + asize is automatically true. Only d < bsize needs testing.
- **a == b:** both inequalities reduce to asize ≠ 0 and bsize ≠ 0.

```asm
; box_overlap: AABB overlap test
; Box A: $00=x $01=y $02=w $03=h
; Box B: $04=x $05=y $06=w $07=h
; Output: C=1 if the boxes overlap, C=0 if not.
;
; Per axis (a < b+bs and b < a+as, no wraparound), d = a - b:
;   a <  b : need (b - a) < as          (tested as as + d: C=1 and nonzero)
;   a >  b : need d < bs
;   a == b : need as != 0 and bs != 0   (bs != 0 via 0 < bs)

box_overlap:
        ; ---- X axis ----
        lda $00
        sec
        sbc $04                 ; A = ax - bx
        bcc @xneg               ; ax < bx
        bne @xpos               ; ax > bx
        ldx $02                 ; ax == bx: aw must be nonzero
        beq @fail
@xpos:
        cmp $06                 ; d < bw ?
        bcs @fail
        bcc @y                  ; always taken
@xneg:
        adc $02                 ; C clear: aw - (bx - ax)
        bcc @fail
        beq @fail
@y:
        ; ---- Y axis ----
        lda $01
        sec
        sbc $05                 ; A = ay - by
        bcc @yneg
        bne @ypos
        ldx $03                 ; ay == by: ah must be nonzero
        beq @fail
@ypos:
        cmp $07                 ; d < bh ?
        bcs @fail
        sec
        rts
@yneg:
        adc $03                 ; C clear: ah - (by - ay)
        bcc @fail
        beq @fail
        rts                     ; C=1 here
@fail:
        clc
        rts
```

Worst-case cycles: 61