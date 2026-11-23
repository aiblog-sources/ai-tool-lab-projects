```asm
PPUCTRL   = $2000
PPUSTATUS = $2002
PPUSCROLL = $2005
PPUADDR   = $2006
PPUDATA   = $2007

vblank_update:
        lda PPUSTATUS           ; reset the $2005/$2006 write latch
        lda #$3F
        sta PPUADDR
        lda #$00
        sta PPUADDR             ; PPU address = $3F00

        ldx #$00
@copy:
        lda $0300,x
        sta PPUDATA
        inx
        cpx #$20
        bne @copy               ; 32 bytes, about 480 cycles, well inside vblank

        ; Leave v outside palette space and restore the scroll position.
        ; Writing $3F00 then $0000 avoids the palette-address glitch on screen.
        lda #$3F
        sta PPUADDR
        lda #$00
        sta PPUADDR
        sta PPUADDR
        sta PPUADDR             ; v = t = $0000: nametable $2000, scroll 0,0
        rts
```

The routine copies `$0300-$031F` to `$3F00-$3F1F`, then writes `$3F00` and `$0000` to `$2006` so the PPU address (v) goes back to `$0000`. That keeps the picture at nametable `$2000` with no scroll while rendering is on. Registers $2000 and $2001 are untouched and no RAM is written.