```asm
vblank_update:
        lda #$00
        sta $2003               ; OAMADDR = 0
        lda #$02
        sta $4014               ; OAM DMA from $0200 (513/514 cycles)

        lda $01                 ; PPUADDR high
        sta $2006
        lda $00                 ; PPUADDR low
        sta $2006

        ; Unrolled copy at 8 cycles/byte (LDA abs = 4, STA $2007 = 4).
        ; This is the fastest possible: the source is in RAM, so every byte
        ; needs a 4-cycle load and a 4-cycle PPU write.
        ; Only the first 208 bytes fit in vblank.
        .repeat 208, i
        lda $0300+i
        sta $2007
        .endrepeat

        lda #$80                ; PPUCTRL = $80
        sta $2000
        lda #$00
        sta $2005               ; scroll X = 0
        sta $2005               ; scroll Y = 0
        rts
```

This routine does not copy all 256 bytes, and no routine can do that inside vblank with rendering on. The OAM DMA costs about 514 cycles, and 256 bytes at the minimum 8 cycles each cost 2048 cycles. That is about 2600 cycles including setup, against roughly 2273 cycles of NTSC vblank.

My previous answer fit 200 bytes, which was a guess. This version fits 208 and leaves about 27 cycles of margin for the NMI entry and the handler's register saves. The last 48 bytes of the destination are still not written.

The only way to copy all 256 bytes is to set PPUMASK to 0 during the copy. That blanks the first couple of scanlines of the next frame and needs extra work to restore the scroll, so it breaks your rule that all PPU work finishes inside vblank. I did not do that. Say if you want that version.

VBlank budget: 2273
Cycles used: 2226