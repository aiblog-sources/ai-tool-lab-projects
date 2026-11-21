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
