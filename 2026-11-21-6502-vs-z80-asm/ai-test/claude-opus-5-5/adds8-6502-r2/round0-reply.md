```asm
; adds8: add signed 8-bit velocity (A) to 16-bit position ($00/$01)
; Clobbers A, X, flags.
adds8:
        tax                 ; set N from velocity (A kept)
        bmi @neg
        clc
        adc $00
        sta $00
        bcc @done           ; no carry out of low byte
        inc $01
@done:
        rts

@neg:
        clc
        adc $00
        sta $00
        bcs @done2          ; carry cancels the $FF sign extension
        dec $01
@done2:
        rts
```

Worst-case cycles: 32

This is the negative-velocity path with no carry out of the low byte: JSR 6, TAX 2, BMI taken 3, CLC 2, ADC 3, STA 3, BCS not taken 2, DEC 5, RTS 6. If the BMI to `@neg` crosses a page boundary at link time, it costs 1 more cycle, making 33.