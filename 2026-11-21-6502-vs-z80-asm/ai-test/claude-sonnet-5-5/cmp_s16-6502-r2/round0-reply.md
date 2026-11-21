```asm
cmp_s16:
        lda $00         ; a low
        cmp $02         ; set carry from low-byte compare (a_lo - b_lo)
        lda $01         ; a high
        sbc $03         ; a_hi - b_hi - borrow, sets N and V
        bvc @nov
        eor #$80        ; overflow: true sign is the inverse of N
@nov:   asl a           ; bit 7 (true sign of a-b) -> carry; set if a < b
        rts
```

Worst-case cycles: 30