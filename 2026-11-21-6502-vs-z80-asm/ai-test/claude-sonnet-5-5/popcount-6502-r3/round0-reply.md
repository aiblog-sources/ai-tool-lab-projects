```asm
popcount:
        tay                     ; save original value
        and #$0F
        tax
        lda popcount_tbl,x      ; bits in low nibble
        sta $00
        tya
        lsr
        lsr
        lsr
        lsr
        tax
        lda popcount_tbl,x      ; bits in high nibble
        clc                     ; carry is unknown after LSR
        adc $00
        rts

popcount_tbl:
        .byte 0,1,1,2,1,2,2,3,1,2,2,3,2,3,3,4
```

The cycle count includes +1 for each table read in case the 16-byte table crosses a page boundary, since I can't guarantee its alignment in the harness.

Worst-case cycles: 48