```asm
; cmp_s16: signed 16-bit compare
; In:  a = $01:$00, b = $03:$02 (two's complement)
; Out: carry set if a < b (signed), clear otherwise
; Method: compute a - b. a < b exactly when N xor V is set.
cmp_s16:
        lda $00
        cmp $02             ; low-byte subtract; carry = no borrow
        lda $01
        sbc $03             ; high byte of a - b; N and V are now valid
        bvc @nov
        eor #$80            ; on overflow, the true sign is inverted N
@nov:
        asl a               ; bit 7 (N xor V) goes into carry
        rts
```

Worst-case cycles: 30