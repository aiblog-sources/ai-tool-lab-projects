Copying all 256 bytes inside vblank with rendering on isn't possible, so this version copies only the first 208 and the last 48 bytes of the destination will still be wrong. The routine that failed was already close to the limit, and the cycle budget explains why.

- **Budget:** NTSC vblank is 20 scanlines × 113.67 ≈ 2273 CPU cycles, and the NMI entry and handler use some of it before the JSR.
- **Fixed cost:** the OAM DMA alone is about 520 cycles, and setting the PPU address and restoring the scroll add about 35 more.
- **Per-byte cost:** each byte needs an absolute load (4 cycles) and a `STA $2007` (4 cycles). Nothing is cheaper, because the data is in RAM at $0300 and self-modifying code is not allowed.
- **Result:** 256 bytes would take 2048 cycles, for about 2600 in total. At most about 210 bytes fit.

I also saved a few cycles in the scroll restore. The two `$2006` writes already reset the write latch, so the second `BIT $2002` was unnecessary.

If you can relax a rule, either of these would fix it:
- Turn rendering off with `PPUMASK=0` during the copy and back on to $1E afterwards. This makes the copy safe outside vblank, but it needs a `PPUMASK` write after the copy.
- Split the copy across two frames.

```asm
VBLANK_UPDATE_COPY_COUNT = 208      ; max that safely fits in NTSC vblank (256 cannot)

vblank_update:
        ; --- OAM DMA from $0200 ---
        lda #$00
        sta $2003
        lda #$02
        sta $4014               ; 513/514 cycles of DMA

        ; --- set PPU address from $00/$01 ---
        bit $2002               ; reset write latch
        lda $01
        sta $2006
        lda $00
        sta $2006

        ; --- unrolled copy, 8 cycles/byte ---
        .repeat VBLANK_UPDATE_COPY_COUNT, i
        lda $0300+i
        sta $2007
        .endrepeat

        ; --- restore scroll and PPUCTRL (write latch is already clear
        ;     after the two $2006 writes; $2007 does not touch it) ---
        lda #$00
        sta $2005               ; X scroll = 0
        sta $2005               ; Y scroll = 0
        lda #$80
        sta $2000               ; nametable $2000, NMI on, inc 1
        rts
```

VBlank budget: 2273
Cycles used: 2230