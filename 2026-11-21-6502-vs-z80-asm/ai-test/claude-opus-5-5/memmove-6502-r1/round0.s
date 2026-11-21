; memmove: copy X bytes from ($00) to ($02), safe for overlap in both directions.
;   dst <  src : copy forward  (lowest address first)
;   dst >= src : copy backward (highest address first)
; Clobbers A, X, Y, flags and $00-$03.

memmove:
        txa
        beq @done               ; count = 0: nothing to do
        tay                     ; Y = count
        lda $02                 ; 16-bit compare dst - src
        cmp $00
        lda $03
        sbc $01
        bcc @fwd                ; dst < src: copy forward

; ---- backward copy: pointers -= 1, then Y = count..1 ----
        lda $00
        bne @b1
        dec $01
@b1:    dec $00
        lda $02
        bne @b2
        dec $03
@b2:    dec $02
@bloop: lda ($00),y
        sta ($02),y
        dey
        bne @bloop
@done:  rts

; ---- forward copy: ptr += count - 256, then Y = 256-count..255 ----
@fwd:                           ; carry is clear here
        txa
        adc $00
        sta $00
        bcs @f1                 ; carry out: +256-256, high byte unchanged
        dec $01                 ; no carry: high byte - 1
@f1:    clc
        txa
        adc $02
        sta $02
        bcs @f2
        dec $03
@f2:    txa
        eor #$FF
        tay
        iny                     ; Y = 256 - count
@floop: lda ($00),y
        sta ($02),y
        iny
        bne @floop
        rts
