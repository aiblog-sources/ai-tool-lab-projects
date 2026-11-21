```asm
; adds8: add signed 8-bit velocity (A) to 16-bit position ($00 low, $01 high)
adds8:
        cmp #$80            ; C = 1 if A is negative (A >= $80)
        bcc @pos
        ; negative velocity: high byte gets $FF plus the carry
        clc
        adc $00
        sta $00
        bcs @done           ; carry cancels the $FF: high is unchanged
        dec $01             ; no carry: high += $FF, which is high - 1
@done:
        rts
@pos:
        ; positive velocity: C is already clear from the CMP
        adc $00
        sta $00
        bcc @done2
        inc $01
@done2:
        rts
```

A negative velocity needs $FF added to the high byte. So the high byte is decremented when the low add produces no carry, and left alone when it does. A positive velocity just passes its carry into the high byte. Both cases wrap at 16 bits.

Worst-case cycles: 31