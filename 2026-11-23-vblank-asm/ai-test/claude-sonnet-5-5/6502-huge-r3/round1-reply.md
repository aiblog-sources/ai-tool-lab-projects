I can't make this routine copy all 256 bytes under your rules. The code below is the same as before, because the 56 wrong bytes come from the cycle budget and not from a bug.

- **Budget:** an NTSC vblank is about 20 scanlines × 113.67 ≈ 2273 CPU cycles.
- **Cost of the DMA:** the OAM DMA takes 513 or 514 cycles. I assume 514.
- **Cost of the copy:** each byte needs a load (`lda abs`, 4 cycles) and a store to $2007 (`sta abs`, 4 cycles), so 8 cycles per byte. I found no cheaper way to get bytes from $0300 into $2007.
- **Total:** 256 bytes cost 2048 cycles, so DMA plus copy is about 2560 cycles, well over 2273. At most about 205 bytes fit once the NMI entry, handler saves, PPU address setup and scroll restore are counted.

The 200 bytes I copy end at 550 + 1600 + 20 = 2170 cycles after the JSR, which leaves a little margin for the NMI entry and the handler's register saves. The last 56 bytes are not copied.

To copy all 256 bytes, one of these has to change:

- **Turn rendering off:** write PPUMASK = 0 for the copy and restore $1E afterwards. This breaks your rule that all PPU work finishes inside vblank, and it blanks or glitches the top of the frame.
- **Split the copy across two frames:** this needs persistent state outside $10-$1F, or a caller that passes the second half separately.
- **Shrink the DMA or the copy:** for example, copy fewer bytes per frame.

Tell me which of these you want and I'll write it.

```asm
VBLANK_UPDATE_COPY_COUNT = 200      ; max that fits in vblank (256 does not)

vblank_update:
        ; --- OAM DMA from $0200 ---
        lda #$00
        sta $2003
        lda #$02
        sta $4014               ; 513/514 cycles

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

        ; --- restore scroll and PPUCTRL ---
        bit $2002               ; reset write latch
        lda #$00
        sta $2005               ; X scroll = 0
        sta $2005               ; Y scroll = 0
        lda #$80
        sta $2000               ; nametable $2000, NMI on, inc 1
        rts
```

VBlank budget: 2273
Cycles used: 2170