memmove:
        cpx #0
        beq @done
        lda $02             ; 16-bit compare dst - src
        cmp $00
        lda $03
        sbc $01
        bcc @fwd            ; dst < src: forward copy is safe
        txa                 ; dst >= src: copy backward
        tay
@bwd:   dey
        lda ($00),y
        sta ($02),y
        cpy #0
        bne @bwd
@done:  rts
@fwd:   ldy #0
@floop: lda ($00),y
        sta ($02),y
        iny
        dex
        bne @floop
        rts
