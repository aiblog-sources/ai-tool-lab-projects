; adds8: HL = HL + sign_extend(A)
; In:  HL = position, A = signed velocity
; Out: HL = new position (wraps at 16 bits)
; Clobbers: A, DE, flags
adds8:
        ld      e,a         ; E = low byte of velocity
        rla                 ; carry = sign bit of velocity
        sbc     a,a         ; A = $00 if positive, $FF if negative
        ld      d,a         ; DE = sign-extended velocity
        add     hl,de       ; HL += DE, wraps at 16 bits
        ret
