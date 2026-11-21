memmove:
        cpx #0
        beq @done
        lda $02             ; compare dst - src (16-bit)
        cmp $00
        lda $03
        sbc $01
        bcc @forward        ; dst < src: copy upward
@backward:                  ; dst >= src: copy downward
        txa
        tay
@b:     dey
        lda ($00),y
        sta ($02),y
        cpy #0
        bne @b
@done:  rts
@forward:
        ldy #0
@f:     lda ($00),y
        sta ($02),y
        iny
        dex
        bne @f
        rts
