```asm
; vblank_update
; OAM DMA from $0200, then copy 160 bytes from $0300-$039F to the PPU
; address held in $00/$01. Then restore scroll and PPUCTRL.
;
; Cycle budget: the DMA takes 513 cycles. The copy is fully unrolled at
; 8 cycles per byte (LDA abs + STA $2007), so 1280 cycles. Setup and
; cleanup add about 40 cycles. The total is about 1835 cycles, which fits
; in the 2273-cycle NTSC vblank even after NMI entry overhead.

PPUCTRL   = $2000
PPUSCROLL = $2005
PPUADDR   = $2006
PPUDATA   = $2007
PPUSTATUS = $2002
OAMADDR   = $2003
OAMDMA    = $4014

vblank_update:
        ; --- OAM DMA from page $02 ---
        lda #$00
        sta OAMADDR
        lda #$02
        sta OAMDMA              ; CPU stalled ~513 cycles

        ; --- set PPU destination address ---
        lda PPUSTATUS           ; reset the $2005/$2006 write latch
        lda $01
        sta PPUADDR
        lda $00
        sta PPUADDR

        ; --- unrolled copy of 160 bytes ---
        .repeat 160, i
        lda $0300+i
        sta PPUDATA
        .endrep

        ; --- restore scroll (no scrolling, nametable $2000) ---
        ; The $2006 writes clobbered the internal t register.
        lda PPUSTATUS           ; reset the write latch
        lda #$00
        sta PPUSCROLL           ; X scroll = 0
        sta PPUSCROLL           ; Y scroll = 0
        lda #$80
        sta PPUCTRL             ; NMI on, nametable $2000, nametable bits in t restored
        rts
```

The routine does not use any scratch RAM. The scroll restore at the end matters because rendering is on and the `$2006` writes would otherwise leave the scroll position corrupted for the next frame.