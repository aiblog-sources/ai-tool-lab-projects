; cmp_s16: signed 16-bit compare
; In:  a = $01:$00, b = $03:$02 (two's complement)
; Out: C = 1 if a < b (signed), else C = 0
; Computes a - b. The signed result is negative when N xor V is 1.
; That bit is moved into bit 7 of A, then ASL shifts it into the carry.
cmp_s16:
        lda $00
        cmp $02         ; low byte: C = no borrow
        lda $01
        sbc $03         ; high byte with borrow, sets N and V
        bvc @nov
        eor #$80        ; on overflow, the true sign is the inverse of N
@nov:
        asl a           ; C = bit 7 = (a < b)
        rts
