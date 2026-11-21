```asm
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
```

Worst-case cycles: 19

This is JSR 6 + TAX 2 + LDA abs,X 5 + RTS 6. The LDA takes 5 cycles only when the index crosses a page. The table's alignment inside the harness's CODE segment can't be controlled, so 19 is the worst case. If the table happens to sit on a page boundary, every call takes 18 cycles.