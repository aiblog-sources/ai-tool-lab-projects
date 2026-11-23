```asm
vblank_update:
        lda $2002               ; reset the PPU address latch
        lda #$3F
        sta $2006
        ldx #$00
        stx $2006               ; PPU address = $3F00

@copy:  lda $0300,x
        sta $2007
        inx
        cpx #32
        bne @copy

        ; Park the PPU address outside the palette ($3F00 first, to avoid
        ; palette corruption glitches), then restore it for no scroll.
        lda #$3F
        sta $2006
        lda #$00
        sta $2006               ; v = $3F00
        sta $2006
        sta $2006               ; v = t = $0000 (nametable $2000, scroll 0,0)
        sta $2005
        sta $2005               ; scroll X = 0, Y = 0
        rts
```

The copy is 32 iterations, about 300 cycles, so it fits easily in vblank. Writing $0000 to $2006 and then 0,0 to $2005 afterwards leaves the scroll at 0,0 on nametable $2000, which is what the game expects. PPUCTRL and PPUMASK are not touched.