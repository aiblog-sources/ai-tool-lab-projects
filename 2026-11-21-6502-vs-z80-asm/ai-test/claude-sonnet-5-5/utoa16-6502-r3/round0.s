utoa16:
        ldx #0                  ; table index (10000, 1000, 100, 10, 1)
        ldy #0                  ; output index
@digit:
        lda #'0'
        sta $04                 ; digit counter
@sub:
        lda $00
        sec
        sbc utoa16_lo,x
        sta $05                 ; tentative low byte
        lda $01
        sbc utoa16_hi,x
        bcc @done               ; went negative: digit finished
        sta $01
        lda $05
        sta $00
        inc $04
        jmp @sub
@done:
        lda $04
        sta ($02),y
        iny
        inx
        cpx #5
        bne @digit
        rts

utoa16_lo:
        .byte <10000, <1000, <100, <10, <1
utoa16_hi:
        .byte >10000, >1000, >100, >10, >1
