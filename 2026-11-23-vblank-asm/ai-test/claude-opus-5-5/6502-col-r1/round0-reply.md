```asm
; vblank_update: copy $0300-$031D down nametable 0, column [$00].
; Uses PPUCTRL increment-32 mode, then restores PPUCTRL=$80, scroll 0,0.

vblank_update:
        lda $2002               ; reset PPU address/scroll latch
        lda #$84                ; NMI on, NT $2000, VRAM increment 32
        sta $2000
        lda #$20                ; PPU address = $2000 + column
        sta $2006
        lda $00                 ; column 0..31
        sta $2006

        .repeat 30, I           ; unrolled: 30 bytes down the column
        lda $0300+I
        sta $2007
        .endrepeat

        lda #$80                ; restore PPUCTRL: NMI on, NT $2000, inc 1
        sta $2000
        lda #$00                ; scroll X=0, Y=0 (latch is at first write
        sta $2005               ; after the even number of $2006 writes)
        sta $2005
        rts
```

VBlank budget: 2273
Cycles used: 285