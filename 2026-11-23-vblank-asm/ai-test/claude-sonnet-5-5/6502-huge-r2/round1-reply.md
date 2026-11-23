The full 256-byte copy can't be done in NTSC vblank, so this fix doesn't make it complete: it still copies only the first 200 bytes. The 56 wrong bytes in each test case are the bytes beyond that cutoff.

The cycle budget shows why:
- NTSC vblank is 20 scanlines × ~113.67 cycles, about 2273 cycles.
- The OAM DMA takes 513 or 514 cycles, so about 1750 cycles remain.
- The fastest possible copy is one `LDA abs` (4) plus one `STA $2007` (4), which is 8 cycles per byte. 256 bytes need 2048 cycles, and the DMA plus 256 bytes comes to about 2560, which is more than the whole vblank.
- `PLA`, `LDX`/`STX` and similar variants also cost 8 cycles per byte, and an immediate-operand copy would need self-modifying code, which isn't allowed.
- The two ways to get all 256 bytes across would break your rules: turning rendering off, or writing to the PPU after vblank ends. Skipping or reordering the DMA would also break them.

I kept the copy at 200 bytes because it leaves roughly 117 cycles of slack for NMI entry latency and the handler's register saves, which are not counted in the figure below. About 208 bytes is the theoretical limit, but that leaves almost no margin. The rest of the destination is left untouched, and the scroll and PPUCTRL are restored inside vblank as required.

```asm
; vblank_update: OAM DMA, then a partial copy of $0300-$03FF to the PPU.
; The full 256-byte copy cannot fit in NTSC vblank after the OAM DMA
; (8 cycles/byte minimum => 2048 + ~540 > ~2273), so only the first
; VBLANK_COPY_BYTES bytes are copied.

PPUCTRL   = $2000
PPUSCROLL = $2005
PPUADDR   = $2006
PPUDATA   = $2007
OAMDMA    = $4014

VBLANK_COPY_BYTES = 200

vblank_update:
        lda #$02
        sta OAMDMA              ; OAM DMA from $0200 (513/514 cycles)

        lda $01
        sta PPUADDR
        lda $00
        sta PPUADDR             ; destination = ($01:$00)

        ; Unrolled: LDA abs (4) + STA abs (4) = 8 cycles per byte
        .repeat VBLANK_COPY_BYTES, i
        lda $0300+i
        sta PPUDATA
        .endrepeat

        ; Restore scroll X=0, Y=0 and nametable $2000
        lda #$00
        sta PPUSCROLL
        sta PPUSCROLL
        lda #$80
        sta PPUCTRL
        rts
```

VBlank budget: 2273
Cycles used: 2156