Comparing HL and DE as unsigned gives the right answer when their signs match. When the signs differ, a < b exactly when a is the negative one.

```asm
; cmp_s16: signed 16-bit compare
; In:  HL = a, DE = b (two's complement)
; Out: carry set if a < b (signed), clear otherwise
; Clobbers: A, HL, other flags
cmp_s16:
        ld   a,h
        xor  d                  ; bit 7 = signs differ; XOR also clears carry
        jp   m,.diff
        sbc  hl,de              ; same signs: unsigned borrow = signed less-than
        ret
.diff:
        ld   a,h
        rla                     ; carry = sign of a (a negative means a < b)
        ret
```

Worst-case cycles: 60