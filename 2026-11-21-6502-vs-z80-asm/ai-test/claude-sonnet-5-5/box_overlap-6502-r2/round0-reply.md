```asm
box_overlap:
        lda $02             ; aw == 0 -> no overlap
        beq @no
        lda $06             ; bw == 0
        beq @no
        lda $03             ; ah == 0
        beq @no
        lda $07             ; bh == 0
        beq @no

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
Worst-case cycles: 103