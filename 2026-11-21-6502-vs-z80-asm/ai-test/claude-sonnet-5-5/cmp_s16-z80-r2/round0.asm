cmp_s16:
        ld   a,h
        xor  $80
        ld   h,a
        ld   a,d
        xor  $80        ; XOR clears carry
        ld   d,a        ; LD doesn't touch flags
        sbc  hl,de      ; carry = (a^$8000) < (b^$8000) unsigned = signed a<b
        ret
