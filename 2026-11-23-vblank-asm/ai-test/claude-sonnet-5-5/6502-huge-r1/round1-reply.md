I can't fix this fully. Copying all 256 bytes can't fit in vblank when rendering stays on, so this version still leaves the last 56 bytes uncopied.

- NTSC vblank is about 2273 CPU cycles (20 scanlines × 113.67).
- The OAM DMA takes 513/514 cycles. The JSR, the handler's register saves and the PPU setup add roughly another 80.
- The 256-byte copy needs at least 8 cycles per byte (LDA abs = 4, STA $2007 = 4), which is 2048 cycles. The total is about 2650, well over the budget.
- I found no way to go below 8 cycles per byte. Unrolling, using X or Y, or using the stack all cost the same.
- The code below is the same as before. The 200 bytes it copies are correct, and the 56 wrong bytes in your tests are exactly the uncopied tail. It's about the limit: roughly 205 bytes fit, so I kept 200 for margin.

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
        ; Only ~200 bytes fit in vblank after the OAM DMA.
        .repeat 200, i
        lda $0300+i
        sta $2007
        .endrepeat

        lda #$80                ; PPUCTRL = $80 (also clears nametable bits in t)
        sta $2000
        lda #$00
        sta $2005               ; scroll X = 0
        sta $2005               ; scroll Y = 0
        rts
```

To copy all 256 bytes, one of these has to change:
- Split the copy across two frames. That needs state kept outside $10-$1F, which the rules forbid.
- Turn rendering off for part of the copy. That breaks the "finish inside vblank" rule and blanks the top of the frame.
- Ask for 8 cycles per byte or better from a different data layout (for example, a smaller update).

VBlank budget: 2273
Cycles used: 2162