```asm
; mul8: unsigned 8x8 -> 16 multiply
; in:  $00 = multiplier, $01 = multiplicand
; out: $02 = product low, $03 = product high
mul8:
        lda $00
        sta $02         ; $02 = multiplier, also collects the product low byte
        lda #0          ; A = running high byte
        ldx #8
        lsr $02         ; C = multiplier bit 0
@loop:
        bcc @noadd
        clc
        adc $01         ; high += multiplicand (carry-out kept in C)
@noadd:
        ror a           ; shift 9-bit high part right; bit 0 goes to C
        ror $02         ; product bit in at the top, next multiplier bit out to C
        dex
        bne @loop
        sta $03
        rts
```
Worst-case cycles: 181