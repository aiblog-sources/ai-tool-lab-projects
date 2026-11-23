```asm
vblank_update:
        lda $2002               ; reset PPU address/scroll latch
        lda #$3F
        sta $2006
        lda #$00
        sta $2006               ; PPU address = $3F00
        ldx #$00
@pal:
        lda $0300,x
        sta $2007
        inx
        cpx #$20
        bne @pal
        lda #$00
        sta $2006               ; t/v = 0 (nametable $2000, scroll 0,0)
        sta $2006
        sta $2005               ; X scroll = 0 (fine X)
        sta $2005               ; Y scroll = 0
        lda #$80
        sta $2000               ; PPUCTRL = $80
        rts
```

VBlank budget: 2273
Cycles used: 527