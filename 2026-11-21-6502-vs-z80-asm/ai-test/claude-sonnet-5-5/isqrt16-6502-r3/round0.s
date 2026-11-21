; isqrt16: A = floor(sqrt(n)), n = $00 (low), $01 (high)
; Scratch: $02/$03 = remainder, $04 = root, $05/$06 = trial divisor

isqrt16:
        lda #0
        sta $02
        sta $03
        sta $04
        ldx #8
@loop:
        asl $00             ; move the top 2 bits of n into the remainder
        rol $01
        rol $02
        rol $03
        asl $00
        rol $01
        rol $02
        rol $03

        lda $04             ; trial = root*4 + 1 (16-bit)
        sta $05
        lda #0
        sta $06
        asl $05
        rol $06
        asl $05
        rol $06
        inc $05

        lda $02             ; remainder - trial
        sec
        sbc $05
        tay
        lda $03
        sbc $06
        bcc @no             ; remainder < trial: root bit = 0
        sta $03
        sty $02             ; carry is still set: root bit = 1
@no:
        rol $04             ; root = root*2 + carry
        dex
        bne @loop
        lda $04
        rts
