No routine can copy all 256 bytes and also finish inside vertical blank, so this one copies only as many bytes as fit.

**Why the full copy cannot fit:**
- Each byte needs a `STA $2007`, which takes at least 4 cycles.
- The value has to be loaded from page $03 first. With unknown data, that load takes at least 4 cycles (`LDA abs`, `PLA` and `LAX` all cost 4).
- So each byte costs at least 8 cycles, and 256 bytes cost 2048 cycles.
- The OAM DMA plus PPU setup add about 550 cycles, and the NMI entry before the JSR adds about 11.
- That totals about 2610 cycles, but vertical blank ends 2263 cycles after the handler starts (about 2273 cycles from its real start).
- About 45 bytes are always left over, which matches the 43–44 wrong bytes in your tests.

Writing to $2007 after vertical blank, as the previous version did, also corrupts VRAM and the scroll while the screen is drawing.

This version does the OAM DMA, then copies the first **210 bytes** ($0300–$03D1). It resets PPUCTRL and the scroll, and every write ends inside vertical blank. The last 46 bytes ($03D2–$03FF) are **not** written.

You can get all 256 bytes each frame only by relaxing one rule:
- **Split the copy across two frames.** This needs one byte of RAM that keeps its value between frames, outside $10–$1F.
- **Use forced blank.** Write $00 to PPUMASK, finish the last ~46 bytes, then write $1E back. This breaks the "inside vblank" rule and shows about 4 black scanlines at the top of the screen.
- **Make the transfer smaller:** about 210 bytes or fewer.

```asm
; vblank_update -- OAM DMA from $0200, then copy as much of $0300-$03FF to the
; PPU (address in $00/$01) as fits in NTSC vblank.
; Every unknown byte needs at least 8 cycles (LDA abs 4 + STA $2007 4), so the
; full 256 bytes (~2610 cycles with the DMA) cannot fit in vblank (~2263 cycles
; after the NMI handler starts). This copies the first 210 bytes and leaves all
; PPU writes inside vblank. Bytes $03D2-$03FF are NOT copied.

vblank_update_COUNT = 210

vblank_update:
        lda #$00                ; 2
        sta $2003               ; 4   OAMADDR = 0
        lda #$02                ; 2
        sta $4014               ; 4 + 513/514  OAM DMA from $0200

        bit $2002               ; 4   reset write latch
        lda $01                 ; 3
        sta $2006               ; 4   PPUADDR high
        lda $00                 ; 3
        sta $2006               ; 4   PPUADDR low (increment 1)

        .repeat vblank_update_COUNT, I
        lda $0300+I             ; 4
        sta $2007               ; 4
        .endrepeat

        lda #$80                ; 2
        sta $2000               ; 4   PPUCTRL = $80, nametable $2000
        lda #$00                ; 2
        sta $2005               ; 4   scroll X = 0
        sta $2005               ; 4   scroll Y = 0
        rts
```

VBlank budget: 2273
Cycles used: 2246