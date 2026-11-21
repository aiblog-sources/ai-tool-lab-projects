```asm
; box_overlap: AABB overlap test
; In:  box A at $00-$03 (x,y,w,h), box B at $04-$07
; Out: carry set = overlap, clear = no overlap
; Clobbers A, flags, and zero-page scratch $08

box_overlap:
        lda $02             ; aw = 0 -> no overlap
        beq @no
        lda $06             ; bw = 0 -> no overlap
        beq @no
        lda $03             ; ah = 0 -> no overlap
        beq @no
        lda $07             ; bh = 0 -> no overlap
        beq @no

        ; ax < bx + bw
        clc
        lda $04
        adc $06
        bcs @c1             ; sum >= 256, so always greater than ax
        sta $08
        lda $00
        cmp $08
        bcs @no             ; ax >= sum
@c1:
        ; bx < ax + aw
        clc
        lda $00
        adc $02
        bcs @c2
        sta $08
        lda $04
        cmp $08
        bcs @no
@c2:
        ; ay < by + bh
        clc
        lda $05
        adc $07
        bcs @c3
        sta $08
        lda $01
        cmp $08
        bcs @no
@c3:
        ; by < ay + ah
        clc
        lda $01
        adc $03
        bcs @yes
        sta $08
        lda $05
        cmp $08
        bcs @no
@yes:
        sec
        rts
@no:
        clc
        rts
```

Worst-case cycles: 119