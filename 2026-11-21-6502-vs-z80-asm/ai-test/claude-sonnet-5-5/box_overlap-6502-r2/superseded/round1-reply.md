The 63 failures were all my zero-size checks. The tests expect the four inequalities alone to decide the result. For example, a=[5,0,0,8] against b=[0,4,20,8] satisfies all four, so the expected carry is 1 even though aw=0. I removed the early-outs and kept only the four comparisons.

```asm
box_overlap:
        ; ax < bx + bw
        lda $04
        clc
        adc $06
        bcs @c1             ; sum >= 256 > ax, always true
        cmp $00
        beq @no             ; sum == ax -> fail
        bcc @no             ; sum <  ax -> fail
@c1:
        ; bx < ax + aw
        lda $00
        clc
        adc $02
        bcs @c2
        cmp $04
        beq @no
        bcc @no
@c2:
        ; ay < by + bh
        lda $05
        clc
        adc $07
        bcs @c3
        cmp $01
        beq @no
        bcc @no
@c3:
        ; by < ay + ah
        lda $01
        clc
        adc $03
        bcs @yes
        cmp $05
        beq @no
        bcc @no
@yes:
        sec
        rts
@no:
        clc
        rts
```

Worst-case cycles: 82