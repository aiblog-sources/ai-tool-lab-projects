```asm
utoa16:
        ld      bc,-10000
        call    .digit
        ld      bc,-1000
        call    .digit
        ld      bc,-100
        call    .digit
        ld      bc,-10
        call    .digit
        ld      a,l
        add     a,'0'
        ld      (de),a
        ret

; HL = remainder, BC = -power of ten, DE = output pointer
.digit:
        ld      a,'0'-1
.loop:
        inc     a
        add     hl,bc           ; carry set while HL >= power
        jr      c,.loop
        sbc     hl,bc           ; carry is clear here: HL = HL + power (restore)
        ld      (de),a
        inc     de
        ret
```
Worst-case cycles: 1285