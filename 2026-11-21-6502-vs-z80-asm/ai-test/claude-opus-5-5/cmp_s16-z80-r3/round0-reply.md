```asm
; cmp_s16: signed 16-bit less-than
; In:  HL = a, DE = b (two's complement)
; Out: carry set if a < b (signed), clear otherwise
; Method: compute HL - DE in A. Signed less-than = S xor V.
; If V is set, flip bit 7. Then RLA moves bit 7 into carry.
cmp_s16:
        ld   a,l
        sub  e              ; low byte: L - E, sets the borrow
        ld   a,h
        sbc  a,d            ; high byte: H - D - borrow; S and V are now valid
        jp   po,.no_ovf     ; V clear: the sign bit already holds the answer
        xor  $80            ; V set: the true sign is the opposite
.no_ovf:
        rla                 ; carry = bit 7 = (S xor V) = (a < b)
        ret
```
Worst-case cycles: 64