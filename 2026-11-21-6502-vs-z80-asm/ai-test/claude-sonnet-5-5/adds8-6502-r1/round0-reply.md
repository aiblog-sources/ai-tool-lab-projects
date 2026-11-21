```asm
adds8:
        tax                 ; save velocity
        clc
        adc $00             ; low += velocity
        sta $00             ; carry = carry out of low byte
        txa                 ; sets N from velocity sign (carry untouched)
        bpl @pos
        lda $01             ; negative: high += $FF + carry
        adc #$FF
        sta $01
        rts
@pos:
        lda $01             ; positive: high += 0 + carry
        adc #$00
        sta $01
        rts
```
Worst-case cycles: 35