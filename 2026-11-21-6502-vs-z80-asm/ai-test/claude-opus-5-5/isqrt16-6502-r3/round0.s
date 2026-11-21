; isqrt16 - floor(sqrt(n)) for unsigned 16-bit n
; In:  $00 = n low, $01 = n high (destroyed)
; Out: A = root (0..255)
; Scratch: $02 = root, $03/$04 = remainder lo/hi, $05/$06 = trial lo/hi
; Method: restoring bit-by-bit square root, 8 iterations of 2 input bits each.

isqrt16:
        lda #0
        sta $02             ; root = 0
        sta $03             ; rem lo = 0
        sta $04             ; rem hi = 0
        ldx #8
@loop:
        ; rem = (rem << 2) | top two bits of n ; n <<= 2
        asl $00
        rol $01
        rol $03
        rol $04
        asl $00
        rol $01
        rol $03
        rol $04

        ; trial = root*4 + 1 (up to 9 bits)
        lda #0
        sta $06
        lda $02
        asl a
        rol $06
        asl a
        rol $06
        ora #1
        sta $05

        ; if rem >= trial then rem -= trial, new root bit = 1
        lda $03
        sec
        sbc $05
        tay
        lda $04
        sbc $06
        bcc @no             ; C=0 -> rem < trial, root bit = 0
        sta $04
        sty $03             ; C=1 here -> root bit = 1
@no:
        rol $02             ; root = root*2 + C
        dex
        bne @loop

        lda $02
        rts
