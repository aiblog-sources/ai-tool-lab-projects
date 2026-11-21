```asm
isqrt16:
        lda #0
        sta $02             ; remainder low
        sta $03             ; remainder high
        sta $04             ; root
        ldx #8
@loop:
        asl $00             ; shift top 2 bits of n into remainder
        rol $01
        rol $02
        rol $03
        asl $00
        rol $01
        rol $02
        rol $03
        lda $04             ; trial = root*4 (+1 via borrow trick)
        asl
        sta $05
        lda #0
        rol
        sta $06
        asl $05
        rol $06
        lda $02
        clc                 ; A - M - 1  => rem - (4*root+1)
        sbc $05
        tay
        lda $03
        sbc $06
        bcc @no             ; rem < trial
        sta $03
        sty $02
        sec
        rol $04             ; root = root*2 + 1
        jmp @next
@no:
        asl $04             ; root = root*2
@next:
        dex
        bne @loop
        lda $04
        rts
```
Worst-case cycles: 857