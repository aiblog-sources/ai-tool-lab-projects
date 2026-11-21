```asm
; utoa16 - convert unsigned 16-bit HL to 5 ASCII decimal digits at (DE)
; In:  HL = value, DE = output pointer
; Out: 5 bytes written at DE..DE+4, most significant digit first
; Clobbers: AF, BC, DE, HL

utoa16:
        ld      bc,-10000
        call    .digit
        ld      bc,-1000
        call    .digit
        ld      bc,-100
        call    .digit
        ld      bc,-10
        call    .digit
        ld      a,l             ; remaining 0..9
        add     a,'0'
        ld      (de),a
        ret

; Subtract -BC from HL as many times as possible.
; Writes '0' plus the count at (DE), then advances DE.
.digit:
        ld      a,'0'-1
.loop:
        inc     a
        add     hl,bc           ; carry set while HL >= |BC|
        jr      c,.loop
        sbc     hl,bc           ; carry is clear here, so this undoes the last add
        ld      (de),a
        inc     de
        ret
```

Worst-case cycles: 1285