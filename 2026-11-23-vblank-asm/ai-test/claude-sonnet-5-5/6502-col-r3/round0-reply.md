```asm
vblank_update:
        lda $2002               ; reset PPU address/scroll latch
        lda #$84                ; NMI on, increment 32 (down a column)
        sta $2000
        lda #$20
        sta $2006               ; high byte of $2000 + column
        lda $00
        sta $2006               ; low byte = column (0-31)
        ldx #0
@loop:
        lda $0300,x
        sta $2007               ; each write advances the address by 32
        inx
        cpx #30
        bne @loop
        lda #$80                ; restore PPUCTRL: increment 1, nametable $2000
        sta $2000
        lda #0
        sta $2005               ; scroll X = 0
        sta $2005               ; scroll Y = 0
        rts
```

VBlank budget: 2273
Cycles used: 496