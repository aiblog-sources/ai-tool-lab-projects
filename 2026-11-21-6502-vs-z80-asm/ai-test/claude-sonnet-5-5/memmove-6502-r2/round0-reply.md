```asm
; memmove: copy X bytes from (src=$00/$01) to (dst=$02/$03), overlap-safe.
; If src < dst, copy backward (high to low); otherwise copy forward.
memmove:
        cpx #0
        beq @done
        lda $00
        cmp $02
        lda $01
        sbc $03
        bcs @fwd            ; src >= dst: forward copy is safe
        txa                 ; src < dst: backward copy
        tay
@bk:    dey
        lda ($00),y
        sta ($02),y
        cpy #0
        bne @bk
        rts
@fwd:   ldy #0
@fl:    lda ($00),y
        sta ($02),y
        iny
        dex
        bne @fl
@done:  rts
```

Worst-case cycles: 4878