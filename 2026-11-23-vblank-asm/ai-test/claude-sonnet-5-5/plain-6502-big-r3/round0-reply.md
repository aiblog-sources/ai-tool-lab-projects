```asm
; vblank_update
; OAM DMA from $0200, then copy 160 bytes from $0300 to PPU at ($01:$00).
; Cost: ~514 cycles for the DMA, then 160 * 8 = 1280 cycles for the copy.
; That is about 1830 cycles in total, which fits inside NTSC vblank (~2273 cycles).
; The copy is unrolled (LDA abs / STA $2007) to stay within that budget.

vblank_update:
        ; --- OAM DMA ---
        lda #$00
        sta $2003               ; OAMADDR = 0
        lda #$02
        sta $4014               ; DMA from $0200

        ; --- set PPU destination address ---
        lda $2002               ; reset the address latch
        lda $01
        sta $2006
        lda $00
        sta $2006

        ; --- unrolled copy of 160 bytes ---
        .repeat 160, i
        lda $0300 + i
        sta $2007
        .endrepeat

        ; --- restore scroll / nametable select (PPUADDR writes clobbered t) ---
        lda #$80
        sta $2000               ; base nametable $2000, NMI on, increment 1
        lda $2002               ; reset the write latch
        lda #$00
        sta $2005               ; X scroll = 0
        sta $2005               ; Y scroll = 0
        rts
```

The copy is unrolled because a looped `LDA abs,X / STA / INX / CPX / BNE` takes about 15 cycles per byte, roughly 2400 cycles in total. That would overrun vblank once the 513-cycle DMA is counted. The unrolled version takes 8 cycles per byte.

After the PPUADDR writes, the routine rewrites `$2000` and `$2005` twice. This puts the internal scroll registers back to nametable 0 with no scroll, so the rendered frame isn't shifted.