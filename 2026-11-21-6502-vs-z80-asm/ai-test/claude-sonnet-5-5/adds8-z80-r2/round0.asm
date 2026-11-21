adds8:
    ld   e,a        ; E = velocity
    add  a,a        ; carry = sign bit
    sbc  a,a        ; A = $00 or $FF (sign extension)
    ld   d,a        ; DE = sign-extended velocity
    add  hl,de      ; HL += DE, wraps at 16 bits
    ret
