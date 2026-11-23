```asm
vblank_update:
        lda $2002               ; reset the PPU write latch
        lda #$3F
        sta $2006               ; PPUADDR high = $3F
        ldx #$00
        stx $2006               ; PPUADDR low = $00 -> $3F00
@loop:
        lda $0300,x
        sta $2007
        inx
        cpx #$20
        bne @loop

        lda #$00                ; point v away from the palette before rendering resumes
        sta $2006
        sta $2006               ; v = t = $0000
        sta $2005               ; scroll X = 0
        sta $2005               ; scroll Y = 0
        lda #$80
        sta $2000               ; PPUCTRL = $80 (NMI on, nametable $2000)
        rts
```

VBlank budget: 2273
Cycles used: 525