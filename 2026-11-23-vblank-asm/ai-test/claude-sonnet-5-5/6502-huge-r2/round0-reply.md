```asm
; vblank_update: OAM DMA, then a partial copy of $0300-$03FF to the PPU.
; WARNING: the full 256-byte copy does not fit in NTSC vblank after the DMA
; (see the notes below), so this copies only the first 200 bytes.

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

I could not do the full job inside vblank, so this routine copies only the first 200 of the 256 bytes. NTSC vblank is about 2273 CPU cycles, and the OAM DMA alone takes 513/514 of them. The fastest possible copy from RAM to `$2007` is 8 cycles per byte (`LDA abs` + `STA abs`), so 256 bytes need 2048 cycles. That would total about 2600 cycles and run past the end of vblank. A copy loop would be even slower, at about 13 cycles per byte.

Doing the full copy would mean either turning rendering off or running past vblank, and both break your rules. I chose to stay inside vblank. The last 56 bytes (`$03C8-$03FF`) are not copied, and the routine keeps no state to resume them next frame. To get all 256 bytes across, you could split the transfer over two frames, which needs RAM outside `$10-$1F` to track progress. You could also shrink the OAM DMA (not possible) or send fewer bytes per frame.

The count of 200 leaves about 100 cycles of slack for NMI entry and the handler's register saves (about 20 cycles) plus DMA alignment jitter.

VBlank budget: 2273
Cycles used: 2156