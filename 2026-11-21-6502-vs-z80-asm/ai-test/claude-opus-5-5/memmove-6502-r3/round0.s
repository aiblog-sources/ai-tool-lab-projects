; memmove: copy X bytes from ($00) to ($02), safe for overlap in either direction.
; If dst < src it copies forward, otherwise it copies backward.
; Scratch: $04 (count), and $00-$03 are modified.

memmove:
        txa
        beq @done               ; count = 0: nothing to do

        ; Compare dst and src as 16-bit values: carry clear means dst < src.
        lda $02
        cmp $00
        lda $03
        sbc $01
        bcc @fwd                ; dst < src: copy forward

        ; ---- Backward copy (dst >= src) ----
        ; Take 1 off each pointer, then copy with Y = count .. 1.
        lda $00
        bne @b1
        dec $01
@b1:    dec $00
        lda $02
        bne @b2
        dec $03
@b2:    dec $02
        txa
        tay
@bl:    lda ($00),y
        sta ($02),y
        dey
        bne @bl
@done:  rts

        ; ---- Forward copy (dst < src) ----
        ; ptr' = ptr + count - 256, then Y runs from 256-count up to 0.
@fwd:   stx $04
        lda $00
        clc
        adc $04
        sta $00
        lda $01
        adc #$FF
        sta $01
        lda $02
        clc
        adc $04
        sta $02
        lda $03
        adc #$FF
        sta $03
        txa
        eor #$FF
        tay
        iny                     ; Y = 256 - count (1..255)
@fl:    lda ($00),y
        sta ($02),y
        iny
        bne @fl
        rts
