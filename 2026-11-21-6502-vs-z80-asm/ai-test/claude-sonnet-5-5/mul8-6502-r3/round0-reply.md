```asm
mul8:
        lda #0              ; high byte accumulator
        ldx #8              ; 8 bits
        lsr $00             ; bit 0 of multiplier -> carry
@loop:
        bcc @noadd
        clc
        adc $01             ; add multiplicand
@noadd:
        ror a               ; shift product right, carry in at top
        ror $00             ; low product in, next multiplier bit out
        dex
        bne @loop
        sta $03             ; high byte
        lda $00
        sta $02             ; low byte
        rts
```

Worst-case cycles: 181