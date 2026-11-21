```asm
; memmove: copy X bytes from ($00) to ($02); handles overlap in both directions.
; If dst >= src the copy runs backward, otherwise it runs forward.
memmove:
        txa
        tay                     ; Y = count
        beq @done               ; count 0: nothing to do
        lda $02                 ; compare dst - src (16-bit)
        cmp $00
        lda $03
        sbc $01
        bcs @back               ; dst >= src: copy backward

; ---- forward copy (dst < src), C = 0 here ----
; Bias both pointers by count-256 so Y runs from 256-count up to 255.
        txa
        eor #$FF
        adc #1                  ; A = 256-count (1..255), C stays 0
        tay
        txa
        adc $00                 ; src += count, high -= 1 unless carry
        sta $00
        bcs @s_ok
        dec $01
@s_ok:  txa
        clc
        adc $02                 ; dst += count, high -= 1 unless carry
        sta $02
        bcs @floop
        dec $03
@floop: lda ($00),y
        sta ($02),y
        iny
        bne @floop
@done:  rts

; ---- backward copy (dst >= src) ----
; Decrement both pointers by 1 so Y runs from count down to 1.
@back:  lda $00
        bne @s_lo
        dec $01
@s_lo:  dec $00
        lda $02
        bne @d_lo
        dec $03
@d_lo:  dec $02
@bloop: lda ($00),y
        sta ($02),y
        dey
        bne @bloop
        rts
```

Worst-case cycles: 4406