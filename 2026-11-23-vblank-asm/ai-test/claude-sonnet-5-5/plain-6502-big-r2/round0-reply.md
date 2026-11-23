```asm
; vblank_update
; Called via JSR from the NMI handler (NTSC, rendering on).
; 1) OAM DMA from $0200 (513 cycles)
; 2) Copy 160 bytes from $0300-$039F to PPU at ($01:$00)
;    The copy is fully unrolled at 8 cycles per byte (LDA abs + STA $2007),
;    about 1280 cycles. Total is about 1850 cycles, inside the ~2270-cycle
;    NTSC vblank. A normal indexed loop at 15 cycles per byte would
;    take about 2400 cycles and overrun vblank.
; 3) Restore the scroll registers, because $2006 writes clobber the PPU's
;    internal t/v registers while rendering is on.

PPUCTRL   = $2000
PPUSTATUS = $2002
OAMADDR   = $2003
PPUSCROLL = $2005
PPUADDR   = $2006
PPUDATA   = $2007
OAMDMA    = $4014

vblank_update:
        ; --- OAM DMA ---
        lda #$00
        sta OAMADDR
        lda #$02
        sta OAMDMA

        ; --- set PPU destination address ---
        lda PPUSTATUS           ; reset the $2005/$2006 write latch
        lda $01
        sta PPUADDR
        lda $00
        sta PPUADDR

        ; --- unrolled copy of 160 bytes ---
        .repeat 160, I
        lda $0300+I
        sta PPUDATA
        .endrepeat

        ; --- restore scroll (no scrolling, nametable $2000) ---
        lda PPUSTATUS           ; reset latch
        lda #$00
        sta PPUSCROLL           ; X scroll = 0
        sta PPUSCROLL           ; Y scroll = 0
        lda #$80
        sta PPUCTRL             ; NMI on, nametable 0 (resets t nametable bits)
        rts
```