```asm
; cmp_s16: signed 16-bit compare
; In:  a = $01:$00, b = $03:$02 (two's complement)
; Out: C = 1 if a < b (signed), else C = 0
cmp_s16:
        lda $00
        cmp $02         ; C = no borrow from low byte
        lda $01
        sbc $03         ; high byte of a - b; N and V valid
        bvc @nov
        eor #$80        ; on overflow the true sign is the inverse of bit 7
@nov:
        asl a           ; bit 7 (= N xor V) goes into the carry
        rts
```

The routine computes a − b over 16 bits and sets the carry to N xor V of the high byte, which is the signed less-than result.

- `CMP` on the low bytes sets the carry to "no borrow", whatever the carry was on entry.
- `SBC` on the high bytes uses that carry to finish the 16-bit subtraction.
- If V is set, bit 7 of the result has the wrong sign, so `EOR #$80` corrects it.
- `ASL` shifts bit 7 into C.

The slowest path is when the branch is not taken: `BVC` (2) plus `EOR` (2) costs 4 cycles. A taken branch costs 3, or 4 if it crosses a page.

Worst-case cycles: 30