```asm
; vblank_update: copy $0300-$031D down nametable 0 column ($00)
; Uses VRAM increment-32 mode so each $2007 write moves down one row.

vblank_update:
        lda #$84                ; NMI on, VRAM increment 32
        sta $2000
        bit $2002               ; reset the $2005/$2006 write latch
        lda #$20                ; high byte: $2000 + column
        sta $2006
        lda $00                 ; low byte = column (0-31)
        sta $2006

        .repeat 30, I           ; unrolled copy, 8 cycles per byte
        lda $0300+I
        sta $2007
        .endrepeat

        lda #$80                ; restore: NMI on, nametable $2000, inc 1
        sta $2000
        lda #$00
        sta $2005               ; scroll X = 0
        sta $2005               ; scroll Y = 0
        rts
```
VBlank budget: 2273
Cycles used: 285