popcount:
        tay                     ; Y = input byte
        lda popcount_tbl,y      ; 256-entry table lookup
        rts

popcount_tbl:
        .repeat 256, N
        .byte ((N >> 0) & 1) + ((N >> 1) & 1) + ((N >> 2) & 1) + ((N >> 3) & 1) + ((N >> 4) & 1) + ((N >> 5) & 1) + ((N >> 6) & 1) + ((N >> 7) & 1)
        .endrepeat
