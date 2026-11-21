adds8:
        ld   e,a        ; E = velocity (low byte)
        add  a,a        ; carry = sign bit of velocity
        sbc  a,a        ; A = $FF if negative, else $00
        ld   d,a        ; DE = sign-extended velocity
        add  hl,de      ; HL = position + velocity, wraps at 16 bits
        ret
