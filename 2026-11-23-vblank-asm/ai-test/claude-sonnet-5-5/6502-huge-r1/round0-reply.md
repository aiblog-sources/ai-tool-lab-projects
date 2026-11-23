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

        ; Unrolled copy, 8 cycles/byte (LDA abs = 4, STA $2007 = 4).
        ; Only the first 200 bytes fit in vblank (see notes below).
        .repeat 200, i
        lda $0300+i
        sta $2007
        .endrepeat

        lda #$80                ; PPUCTRL = $80, which also sets the nametable bits in t to 0
        sta $2000
        lda #$00
        sta $2005               ; scroll X = 0
        sta $2005               ; scroll Y = 0
        rts
```

**Notes:** The full 256-byte copy cannot fit in one NTSC vblank after the OAM DMA, so this routine copies only the first 200 bytes ($0300-$03C7), not all 256. The remaining 56 bytes are not copied.

- Vblank is about 2273 cycles.
- The OAM DMA takes about 514 cycles, leaving roughly 1700 for the VRAM copy.
- The fastest possible copy from RAM to $2007 is 8 cycles per byte (a 4-cycle load plus a 4-cycle store), so 256 bytes need 2048 cycles.
- DMA plus copy therefore needs about 2560 cycles, which overruns vblank by about 290. Finishing the copy in the visible frame with rendering on would corrupt the PPU address and the picture.

To move all 256 bytes, split the copy across two frames (this needs persistent RAM for the state, which the rules forbid), drop the OAM DMA on some frames, or shrink the buffer to about 200 bytes or fewer.

VBlank budget: 2273
Cycles used: 2162