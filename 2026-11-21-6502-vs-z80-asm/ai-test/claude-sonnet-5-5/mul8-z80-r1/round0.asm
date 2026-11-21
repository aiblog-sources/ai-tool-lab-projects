; mul8: HL = H * E (unsigned). D, B and flags are clobbered.
mul8:
        ld   l,0            ; HL = multiplier (H) in the high byte
        ld   d,0            ; DE = multiplicand (E)
        ld   b,8
.loop:
        add  hl,hl          ; shift the next multiplier bit into carry
        jr   nc,.skip
        add  hl,de          ; add the multiplicand if the bit was set
.skip:
        djnz .loop
        ret
