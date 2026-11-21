utoa16:
        ld      bc,-10000
        call    .digit
        ld      bc,-1000
        call    .digit
        ld      bc,-100
        call    .digit
        ld      c,-10           ; B is still $FF, so BC = -10
        call    .digit
        ld      a,l             ; remainder 0..9
        add     a,'0'
        ld      (de),a
        ret

.digit:
        ld      a,'0'-1
.loop:
        inc     a
        add     hl,bc
        jr      c,.loop
        sbc     hl,bc           ; carry is clear here, so this restores HL
        ld      (de),a
        inc     de
        ret
