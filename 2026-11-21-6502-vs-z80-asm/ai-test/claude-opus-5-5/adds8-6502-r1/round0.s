; adds8: add signed 8-bit velocity (A) to 16-bit position at $00/$01
; Clobbers A and flags. X and Y are not touched.
adds8:
        cmp #$80            ; C=1 if A is negative, C=0 if A is positive
        bcs @neg
                            ; positive: C is already clear
        adc $00
        sta $00
        bcc @done
        inc $01             ; carry into the high byte
@done:
        rts
@neg:
        clc
        adc $00
        sta $00
        bcs @done           ; adding $FF plus carry 1 leaves the high byte unchanged
        dec $01             ; adding $FF plus carry 0 subtracts 1
        rts
