**This cannot fit in NTSC vertical blank as specified.** The routine below is the fastest version that follows the spec, but it runs about 340 cycles past the end of vblank.

**Why it can't fit**
- NTSC vblank is 20 scanlines × 341 dots ÷ 3 ≈ 2273 CPU cycles. About 20–30 of those are used before the `JSR` by NMI entry and the register saves.
- OAM DMA takes 513–514 cycles, and nothing can run during it.
- Each byte copied to `$2007` needs at least `STA $2007` (4 cycles) plus a load of at least 3 cycles. A 3-cycle load would need the data in zero page, which the rules forbid. A fully unrolled `LDA abs` / `STA $2007` pair is 8 cycles per byte, so 256 bytes take 2048 cycles.
- 514 + 2048 + setup comes to about 2614 cycles after the `JSR`, against about 2245 available. The last ~45 bytes and the scroll/PPUCTRL writes would land during rendering of scanlines 0–3. That corrupts VRAM and the scroll.

**Ways to make it work (each needs a change outside this routine)**
- Split the copy over two frames, 128 bytes each. That is about 1100 cycles, which fits easily.
- Do the 256-byte copy on frames without OAM DMA. 2048 cycles plus overhead just fits.
- Extend vblank with forced blanking: keep PPUMASK at 0 for the first ~4 scanlines (hidden in NTSC overscan), then re-enable rendering with a timed mid-frame `$2006`/`$2005` scroll fix. This breaks the "finish inside vblank" rule and needs cycle-exact timing.

```asm
; vblank_update -- fastest spec-compliant version.
; WARNING: ~2614 cycles from JSR; exceeds NTSC vblank (~2273 cycles total).
; See notes: requires splitting the copy (or forced-blank extension) to be safe.

vblank_update:
        ; --- OAM DMA from page $02 ---
        lda #$00
        sta $2003               ; OAMADDR = 0
        lda #$02
        sta $4014               ; OAM DMA, 513/514 cycles

        ; --- set VRAM address from ($00/$01) ---
        bit $2002               ; reset $2005/$2006 write latch
        lda $01
        sta $2006               ; high byte
        lda $00
        sta $2006               ; low byte

        ; --- copy $0300-$03FF to VRAM, fully unrolled (8 cycles/byte) ---
        .repeat 256, I
        lda $0300+I
        sta $2007
        .endrepeat

        ; --- restore scroll 0,0 and PPUCTRL $80 (nametable $2000) ---
        lda #$00
        sta $2005               ; X scroll = 0
        sta $2005               ; Y scroll = 0
        lda #$80
        sta $2000               ; NMI on, nametable $2000, inc 1
        rts
```

VBlank budget: 2273
Cycles used: 2614