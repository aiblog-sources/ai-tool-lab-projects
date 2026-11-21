```asm
; popcount: A = number of set bits in A (0..8)
; Uses a 256-entry lookup table in ROM. Clobbers X.
popcount:
        tax                     ; 2
        lda popcount_table,x    ; 4 (+1 if page crossed)
        rts                     ; 6

popcount_table:
.repeat 256, I
        .byte ((I >> 0) & 1) + ((I >> 1) & 1) + ((I >> 2) & 1) + ((I >> 3) & 1) + ((I >> 4) & 1) + ((I >> 5) & 1) + ((I >> 6) & 1) + ((I >> 7) & 1)
.endrepeat
```

Worst-case cycles: 19