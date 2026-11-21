; utoa16: convert HL (unsigned 16-bit) into 5 ASCII decimal digits at DE
; In:  HL = number, DE = output pointer
; Out: 5 bytes '0'..'9' at (DE), most significant digit first
; Clobbers: AF, BC, DE, HL

utoa16:
        ld      bc,-10000
        call    .digit
        ld      bc,-1000
        call    .digit
        ld      bc,-100
        call    .digit
        ld      c,-10           ; B is already $FF
        call    .digit
        ld      a,l             ; remainder 0..9
        add     a,'0'
        ld      (de),a
        ret

; Count how many times -BC can be subtracted from HL.
; Store the count as ASCII at (DE), then advance DE.
.digit:
        ld      a,'0'-1
.loop:
        inc     a               ; INC does not change carry
        add     hl,bc           ; carry set means HL was >= |BC|
        jr      c,.loop
        sbc     hl,bc           ; carry is 0 here, so this undoes the last add
        ld      (de),a
        inc     de
        ret
