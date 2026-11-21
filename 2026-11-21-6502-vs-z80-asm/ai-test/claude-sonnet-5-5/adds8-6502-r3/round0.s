adds8:
        tax                     ; set N from velocity sign
        bpl     @pos
        clc
        adc     $00             ; negative: sign extension is $FF
        sta     $00
        bcs     @done           ; carry set: no borrow, high byte unchanged
        dec     $01             ; carry clear: borrow from high byte
        rts
@pos:
        clc
        adc     $00             ; positive: sign extension is $00
        sta     $00
        bcc     @done
        inc     $01             ; carry out: increment high byte
@done:
        rts
