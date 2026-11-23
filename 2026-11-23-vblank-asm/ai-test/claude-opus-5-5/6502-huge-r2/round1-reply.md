**This cannot be fixed within the rules as given.** The routine was already at the hardware minimum, and the minimum does not fit in NTSC vblank.

**The timing:**

- **Vblank length:** 20 scanlines × 341 dots ÷ 3 ≈ 2273 CPU cycles. Your harness measures 2263 from the handler start, which leaves about 2252 cycles from the JSR.
- **Fixed costs:**
  - JSR: 6
  - OAM DMA with setup: 520 to 526
  - PPUADDR setup: 18
  - PPUCTRL and scroll restore: 16
  - Total: about 566
- **Per-byte cost:** each `$2007` write needs `STA abs`, which takes 4 cycles. Loading a byte from `$0300-$03FF` also takes at least 4 cycles (`LDA abs`, `LDA abs,X` or `PLA`). Only zero-page loads take 3, and the data isn't in zero page. That makes 8 cycles per byte, or 2048 cycles for 256 bytes.
- **Total:** about 2614 cycles, roughly 360 more than the budget. Even at an impossible 7 cycles per byte it would not fit.
- **What does fit:** at most about 210 bytes per vblank after the OAM DMA.

**Ways to make it work.** Each one changes a rule, so you have to choose:

1. **Split the copy over two frames**, for example 128 bytes per frame. This needs one persistent RAM byte to remember the phase, and right now only `$10-$1F` scratch is allowed.
2. **Shrink the per-frame transfer** to about 200 bytes or less.
3. **Use forced blanking.** Write `$2001=0` at the start, finish the writes a few scanlines into the frame, then turn rendering back on with the mid-frame `$2006/$2005` scroll trick. This breaks the "all PPU work inside vblank" rule, and the top 3–4 scanlines show as blank.

Below is the routine at the theoretical minimum. It is correct except for the overrun.

```asm
; vblank_update -- OAM DMA from $0200, then copy $0300-$03FF to PPU ($00/$01)
; This is already the minimum possible: 8 cycles per byte (STA $2007 = 4, and no
; load from $0300 page is cheaper than 4). 256 bytes + OAM DMA ~= 2614 cycles,
; but NTSC vblank is ~2273 cycles. Only ~210 bytes fit after the DMA.
; Meeting the spec needs a rule change (split across frames with a persistent
; RAM byte, smaller transfer, or forced blank).

vblank_update:
        lda #$00                ; 2
        sta $2003               ; 4   OAMADDR = 0
        lda #$02                ; 2
        sta $4014               ; 4 + 513/514  OAM DMA from $0200

        bit $2002               ; 4   reset $2005/$2006 write latch
        lda $01                 ; 3
        sta $2006               ; 4   PPUADDR high
        lda $00                 ; 3
        sta $2006               ; 4   PPUADDR low  (PPUCTRL $80: increment 1)

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