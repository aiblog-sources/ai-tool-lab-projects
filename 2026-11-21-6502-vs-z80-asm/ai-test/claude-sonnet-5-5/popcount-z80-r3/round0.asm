popcount:
        ld      l,a
        ld      h,popcount_tab>>8
        ld      a,(hl)
        ret

        ALIGN   256
popcount_tab:
popcount_n = 0
        DUP 256
        DB ((popcount_n)&1)+((popcount_n>>1)&1)+((popcount_n>>2)&1)+((popcount_n>>3)&1)+((popcount_n>>4)&1)+((popcount_n>>5)&1)+((popcount_n>>6)&1)+((popcount_n>>7)&1)
popcount_n = popcount_n + 1
        EDUP
