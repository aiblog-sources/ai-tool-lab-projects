**This cannot be done within the constraints as stated.** The routine below is the fastest version I can write, but it runs about 360 cycles (roughly 3 scanlines) past the end of vertical blank.

- **Budget:** NTSC vblank is 20 scanlines × 341 dots ÷ 3 ≈ 2273 CPU cycles. About 20–26 of those are spent before the JSR: the NMI entry, finishing the current instruction, and saving A, X and Y. That leaves about 2250 cycles.
- **Fixed costs:** OAM DMA takes 513–514 cycles. Register setup and restore take about 50 more.
- **Copy cost:** Each byte needs a 4-cycle write to `$2007` and at least a 4-cycle load. A 3-cycle `LDA zp` would need the data in zero page, which the RAM rules forbid. So 256 bytes cost at least 2048 cycles.
- **Fit:** Only about 210 bytes fit in vblank, or about 200 with a safety margin.

Ways to make it work:
- Split the copy across two frames (128 bytes each).
- Raise the data limit to about 200 bytes per frame.
- Allow forced blanking, which blacks out the top few scanlines and needs careful handling of the `v` register.

```asm
; vblank_update -- OAM DMA from $0200, then copy $0300-$03FF to PPU ($00/$01)
; Fully unrolled; 8 cycles per byte is the floor for RAM -> $2007.
; NOTE: total exceeds NTSC vblank (see notes) -- does not meet the spec.

vblank_update:
        lda #$00                ; 2
        sta $2003               ; 4   OAMADDR = 0
        lda #$02                ; 2
        sta $4014               ; 4 + 513/514  OAM DMA from $0200

        bit $2002               ; 4   reset $2005/$2006 write latch
        lda $01                 ; 3
        sta $2006               ; 4   PPUADDR high
        lda $00                 ; 3
        sta $2006               ; 4   PPUADDR low  (PPUCTRL still $80: +1)

        .repeat 256, I
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
Cycles used: 2614

That total is 6 (JSR) + 12 + 514 (DMA) + 18 (address setup) + 2048 (copy) + 16 (PPUCTRL/scroll restore). It exceeds the roughly 2250 cycles available after the NMI by about 360 cycles, so the last ~45 `$2007` writes and the scroll reset would land during rendering. Tell me which workaround you'd prefer, and I'll write a version that stays inside vblank.