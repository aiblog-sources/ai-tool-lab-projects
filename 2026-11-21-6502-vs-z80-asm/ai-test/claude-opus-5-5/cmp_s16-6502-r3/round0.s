; cmp_s16: signed 16-bit compare
; In:  a = $01:$00, b = $03:$02 (two's complement)
; Out: C = 1 if a < b (signed), else C = 0
cmp_s16:
        lda $00
        cmp $02         ; C = no borrow from low byte
        lda $01
        sbc $03         ; high byte of a - b; N and V valid
        bvc @nov
        eor #$80        ; on overflow the true sign is the inverse of bit 7
@nov:
        asl a           ; bit 7 (= N xor V) goes into the carry
        rts
