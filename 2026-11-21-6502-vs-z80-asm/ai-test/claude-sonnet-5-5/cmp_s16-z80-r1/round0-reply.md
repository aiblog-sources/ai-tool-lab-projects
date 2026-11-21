```asm
; cmp_s16: signed 16-bit less-than
; In:  HL = a, DE = b (two's complement)
; Out: carry set if a < b, clear otherwise
cmp_s16:
    ld   a,l
    sub  e          ; low byte subtract, sets borrow
    ld   a,h
    sbc  a,d        ; high byte with borrow; S = sign, P/V = 16-bit overflow
    jp   po,.no_ov  ; no overflow: sign bit is already correct
    xor  $80        ; overflow: true sign is the inverse
.no_ov:
    rlca            ; carry = bit 7 = (a < b)
    ret
```
Worst-case cycles: 64