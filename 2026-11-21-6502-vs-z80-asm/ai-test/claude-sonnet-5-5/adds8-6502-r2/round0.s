adds8:
        tax                 ; save velocity (for its sign)
        clc
        adc $00
        sta $00             ; low byte done, carry = unsigned overflow
        txa                 ; reload velocity to test its sign
        bpl @pos
        ; negative velocity: high byte gets $FF added
        ; carry set -> $FF + carry = 0, no change; clear -> decrement
        bcs @done
        dec $01
@done:  rts
@pos:   ; positive velocity: high byte gets 0 + carry
        bcc @done
        inc $01
        rts
