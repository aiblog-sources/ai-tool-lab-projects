adds8:
        ld      e,a         ; E = low byte of velocity
        add     a,a         ; carry = sign bit of velocity
        sbc     a,a         ; A = $00 if positive, $FF if negative
        ld      d,a         ; DE = sign-extended velocity
        add     hl,de       ; HL = position + velocity, wraps at 16 bits
        ret
