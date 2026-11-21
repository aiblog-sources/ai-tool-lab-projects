```asm
; box_overlap: AABB overlap test
; In:  box A at $00-$03 (x,y,w,h), box B at $04-$07
; Out: carry set = overlap, clear = no overlap
; Loops X=1 (y axis) then X=0 (x axis).

box_overlap:
        ldx #1
@loop:
        lda $06,x           ; B size zero -> no overlap
        beq @fail
        lda $02,x           ; A size zero -> no overlap
        beq @fail
        clc
        lda $04,x           ; pos B + size B
        adc $06,x
        bcs @ok1            ; sum > 255, so always > pos A
        sec
        sbc $00,x           ; sum - pos A, must be > 0
        bcc @fail
        beq @fail
@ok1:
        clc
        lda $00,x           ; pos A + size A
        adc $02,x
        bcs @ok2
        sec
        sbc $04,x           ; sum - pos B, must be > 0
        bcc @fail
        beq @fail
@ok2:
        dex
        bpl @loop
        sec
        rts
@fail:
        clc
        rts
```
Worst-case cycles: 137