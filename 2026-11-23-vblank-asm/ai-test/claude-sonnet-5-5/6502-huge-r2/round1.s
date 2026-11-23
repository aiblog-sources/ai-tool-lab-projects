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
