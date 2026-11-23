```asm
vblank_update:
        bit $2002               ; reset PPU address/scroll latch
        lda #$84                ; NMI on, increment 32 (down a column)
        sta $2000
        lda #$20                ; nametable 0 high byte
        sta $2006
        lda $00                 ; column 0-31 = low byte of $2000 + column
        sta $2006
        ldx #0
@loop:
        lda $0300,x
        sta $2007
        inx
        cpx #30
        bne @loop
        lda #$80                ; restore PPUCTRL: increment 1, nametable $2000
        sta $2000
        bit $2002               ; reset latch
        lda #0
        sta $2005               ; X scroll = 0
        sta $2005               ; Y scroll = 0
        rts
```

VBlank budget: 2273
Cycles used: 500