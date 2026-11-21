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
