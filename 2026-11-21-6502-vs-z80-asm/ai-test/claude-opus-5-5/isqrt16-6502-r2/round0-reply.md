```asm
; isqrt16: A = floor(sqrt(n)), where n = $01:$00 (unsigned 16-bit)
; Method: bit-by-bit restoring square root, taking 2 bits of n per step.
; Scratch: $02/$03 = remainder (16-bit), $04 = root, $05/$06 = trial value
; n in $00/$01 is destroyed.

isqrt16:
        lda #0
        sta $02             ; remainder lo
        sta $03             ; remainder hi
        sta $04             ; root
        ldx #8              ; 8 result bits
@loop:
        ; remainder = (remainder << 2) | top two bits of n;  n <<= 2
        asl $00
        rol $01
        rol $02
        rol $03
        asl $00
        rol $01
        rol $02
        rol $03

        ; trial = 4*root + 1   (root <= 127 here, so trial <= 509)
        lda $04
        asl a               ; 2*root, C = 0
        asl a               ; 4*root low byte, C = bit 8
        ora #1
        sta $05
        lda #0
        rol a               ; high byte of trial
        sta $06

        ; if remainder >= trial: remainder -= trial, next root bit = 1
        sec
        lda $02
        sbc $05
        tay
        lda $03
        sbc $06
        bcc @no             ; C = 0 -> next root bit is 0
        sta $03
        sty $02             ; C is still 1 -> next root bit is 1
@no:
        rol $04             ; root = 2*root + C
        dex
        bne @loop

        lda $04
        rts
```

Worst-case cycles: 771