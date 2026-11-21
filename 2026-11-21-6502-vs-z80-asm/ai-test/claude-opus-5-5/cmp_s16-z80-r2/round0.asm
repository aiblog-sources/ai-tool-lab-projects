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
