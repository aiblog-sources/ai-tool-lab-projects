```asm
; isqrt16 - floor(sqrt(n)) for unsigned 16-bit n
; In:  $00 = n low, $01 = n high (both destroyed)
; Out: A = root (0..255)
; Scratch: $02/$03 = remainder, $04 = root, $05/$06 = trial value
; Uses X as the loop counter. Y is used as a temp.

isqrt16:
        lda #0
        sta $02             ; remainder low
        sta $03             ; remainder high
        sta $04             ; root
        ldx #8              ; 8 result bits
@loop:
        ; move the top two bits of n into the remainder
        asl $00
        rol $01
        rol $02
        rol $03
        asl $00
        rol $01
        rol $02
        rol $03

        ; trial base = root << 2 (16-bit, in $05/$06)
        lda #0
        sta $06
        lda $04
        asl
        rol $06
        asl
        rol $06             ; carry now = 0 (old bit 7 of $06)
        sta $05

        ; remainder - (trial base + 1). The carry is clear, so
        ; SBC also subtracts the extra 1.
        lda $02
        sbc $05
        tay
        lda $03
        sbc $06
        bcc @no             ; remainder < trial: this bit is 0
        sta $03             ; keep the new remainder
        sty $02             ; carry is still set: this bit is 1
@no:
        rol $04             ; root = (root << 1) | carry
        dex
        bne @loop

        lda $04
        rts
```
Worst-case cycles: 803