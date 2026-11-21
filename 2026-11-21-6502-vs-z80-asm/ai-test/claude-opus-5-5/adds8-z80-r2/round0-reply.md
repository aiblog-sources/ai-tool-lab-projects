```asm
; adds8: HL = HL + sign-extended A (wraps at 16 bits)
; In:  HL = position, A = signed velocity (-128..127)
; Out: HL = new position
; Clobbers: A, DE, flags
adds8:
        ld      e,a             ; 4   low byte of velocity
        add     a,a             ; 4   carry = sign bit of velocity
        sbc     a,a             ; 4   A = $00 if positive, $FF if negative
        ld      d,a             ; 4   DE = sign-extended velocity
        add     hl,de           ; 11  HL = HL + DE, wraps at 16 bits
        ret                     ; 10
```

Worst-case cycles: 54