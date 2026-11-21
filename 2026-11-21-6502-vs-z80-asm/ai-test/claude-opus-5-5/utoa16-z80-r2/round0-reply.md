```asm
; utoa16 - convert unsigned 16-bit HL to 5 ASCII decimal digits at DE
; In:  HL = value, DE = output pointer
; Out: 5 bytes '0'..'9' written at (DE), most significant digit first
; Clobbers: AF, BC, DE, HL
utoa16:
        ld      bc,-10000
        call    utoa16_digit
        ld      bc,-1000
        call    utoa16_digit
        ld      bc,-100
        call    utoa16_digit
        ld      c,-10           ; B is already $FF from -100 ($FF9C)
        call    utoa16_digit
        ld      a,l             ; remainder 0..9
        add     a,'0'
        ld      (de),a
        ret

; Count how many times |BC| fits into HL by repeated addition of
; the negative power BC. Store the digit at (DE) and advance DE.
; HL is left as the remainder.
utoa16_digit:
        ld      a,'0'-1
.loop:
        inc     a
        add     hl,bc           ; carry set while HL >= |BC|
        jr      c,.loop
        sbc     hl,bc           ; carry is clear here, so this undoes the overshoot
        ld      (de),a
        inc     de
        ret
```

Worst-case cycles: 1282

How the count works:
- A call to `utoa16_digit` that produces digit d costs 27·d + 67 cycles, plus 17 for its CALL.
- The worst case comes from the largest possible sum of the first four digits. That is 59999, with a sum of 32.
- Total: 17 (CALL) + 37 (the BC loads) + 4·(17 + 67) + 27·32 + 28 (last digit and RET) = 1282.