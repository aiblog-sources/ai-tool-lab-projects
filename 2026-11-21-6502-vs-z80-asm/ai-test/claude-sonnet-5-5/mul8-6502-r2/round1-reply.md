The old loop shifted the multiplier in `$00` twice per iteration: once with `lsr $00` and again with `ror $00`, which also took the low product bits. That dropped multiplier bits and gave wrong results for almost all inputs. The fix is to shift the multiplier only with `lsr $00` and collect the low product byte in `$02` with `ror $02`. After 8 rotations every bit of `$02` has been overwritten, so it needs no initialisation.

```asm
; mul8: unsigned 8x8 -> 16 multiply
; In:  $00, $01 = factors
; Out: $02 = product low, $03 = product high
; Clobbers A, X, flags, $00

mul8:
        lda #0              ; high byte accumulator
        ldx #8
@loop:
        lsr $00             ; multiplier bit 0 -> carry
        bcc @noadd
        clc
        adc $01             ; add multiplicand into high accumulator
@noadd:
        ror a               ; shift product right (carry = add overflow)
        ror $02             ; low product bits shift in from A
        dex
        bne @loop
        sta $03
        rts
```

Worst-case cycles: 210