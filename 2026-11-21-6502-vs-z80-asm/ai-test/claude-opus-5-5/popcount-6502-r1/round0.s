; popcount: A = number of set bits in A (0..8)
; Uses a 256-byte ROM lookup table.
popcount:
        tax                     ; 2
        lda popcount_table,x    ; 4 (+1 if page crossed)
        rts                     ; 6

popcount_table:
.repeat 256, i
        .byte ((i>>0)&1)+((i>>1)&1)+((i>>2)&1)+((i>>3)&1)+((i>>4)&1)+((i>>5)&1)+((i>>6)&1)+((i>>7)&1)
.endrepeat
