The task as specified can't be done in one vertical blank. The routine below is the fastest straightforward version, and it runs about 350 cycles past the end of vblank.

**Why it doesn't fit**
- NTSC vblank is 20 scanlines × 341 dots / 3 ≈ **2273 CPU cycles**, counted from the NMI.
- Before your JSR runs, about 20 of those are gone: 7 for the NMI entry and about 13 for saving A, X and Y. That leaves roughly **2253 cycles** from the JSR.
- OAM DMA costs 513 or 514 cycles.
- Each byte copied to `$2007` costs at least 8 cycles: 4 to write `$2007` and 4 to load an unknown value from RAM. `PLA` is no faster, and indexed or indirect modes are slower. So 256 bytes take at least **2048 cycles**.
- 514 + 2048 = 2562 cycles for those two jobs alone, which is more than the whole of vblank.

```asm
; vblank_update -- OAM DMA, then copy $0300-$03FF to VRAM at ($00/$01),
; then restore PPUCTRL=$80 and scroll 0,0.
; WARNING: about 2604 cycles from JSR; NTSC vblank allows about 2253 after
; NMI overhead. The tail of the VRAM copy will land in the pre-render line.
vblank_update:
        lda #$02                ; 2
        sta $4014               ; 4 + 513/514  OAM DMA from $0200

        lda $01                 ; 3  dest high
        sta $2006               ; 4
        lda $00                 ; 3  dest low
        sta $2006               ; 4  (w latch back to 0 after two writes)

        .repeat 256, I          ; fully unrolled: 8 cycles per byte
        lda $0300+I             ; 4
        sta $2007               ; 4
        .endrepeat

        lda #$80                ; 2
        sta $2000               ; 4  NMI on, nametable $2000, inc 1
        lda #$00                ; 2
        sta $2005               ; 4  scroll X = 0
        sta $2005               ; 4  scroll Y = 0
        rts
```

VBlank budget: 2273
Cycles used: 2604

**Ways to make it fit**
1. **Copy half per frame.** Copy 128 bytes each NMI and keep a toggle in a byte from `$10`-`$1F`. Each half costs about 1556 cycles, which fits. This only works if the harness leaves that byte alone between frames.
2. **Copy fewer bytes per frame.** About 200 bytes per frame fits: 6 + 6 + 514 + 14 + 200×8 + 16 ≈ 2156 cycles.
3. **Blank the top of the screen.** Turn rendering off (`$2001` = 0) and turn it back on partway down the frame. This breaks your "next frame unscrolled from `$2000`" rule for the top scanlines.
4. **Do the OAM DMA only on some frames,** for example when sprites have changed. That saves about 520 cycles on frames where it's skipped.

If you tell me which constraint can give, I'll write that version.