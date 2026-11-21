Comparing a sign-adjusted value as unsigned costs 76 cycles. Testing the signs first is faster:

```asm
; cmp_s16: signed 16-bit less-than
; In:  HL = a, DE = b (two's complement)
; Out: carry set if a < b (signed), clear otherwise
; Destroys: A, other flags
cmp_s16:
        ld   a,h
        xor  d                  ; bit 7 set if signs differ
        jp   m,.diff
        ; same sign: an unsigned compare gives the right answer
        ld   a,l
        sub  e
        ld   a,h
        sbc  a,d                ; carry = (HL < DE)
        ret
.diff:
        ; signs differ: a < b exactly when a is negative
        ld   a,h
        rla                     ; carry = sign bit of a
        ret
```

- **Signs differ:** the negative value is the smaller one. Shifting H's sign bit into carry gives the answer.
- **Signs the same:** an unsigned subtract gives the same order as a signed one. `sub e` / `sbc a,d` leaves carry set when HL < DE.

Cycle count for the worst path (signs the same):
CALL 17 + `ld a,h` 4 + `xor d` 4 + `jp m` not taken 10 + `ld a,l` 4 + `sub e` 4 + `ld a,h` 4 + `sbc a,d` 4 + RET 10 = 61.
The signs-differ path takes 53.

Worst-case cycles: 61