```asm
popcount:
        tay                     ; save original
        and #$0F
        tax                     ; X = low nibble
        tya
        lsr a
        lsr a
        lsr a
        lsr a                   ; A = high nibble
        tay
        lda popcount_tab,y      ; bits in high nibble
        clc
        adc popcount_tab,x      ; + bits in low nibble
        rts

popcount_tab:
        .byte 0,1,1,2,1,2,2,3,1,2,2,3,2,3,3,4
```

Worst-case cycles: 42

This counts the JSR (6) and RTS (6). The body is 28 cycles, plus 2 if the 16-byte table crosses a page boundary, which adds one cycle to each indexed load.