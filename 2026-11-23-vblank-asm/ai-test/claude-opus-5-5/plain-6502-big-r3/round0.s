; vblank_update - called from NMI via JSR (NTSC, rendering on)
; 1) OAM DMA from page $02
; 2) Copy $0300-$039F (160 bytes) to PPU address in ($00 lo, $01 hi)
; 3) Restore PPUCTRL / scroll so nametable $2000 shows at (0,0)
; Clobbers A. Uses no RAM.

vblank_update:
        ; --- OAM DMA ---
        lda #$00
        sta $2003               ; OAMADDR = 0
        lda #$02
        sta $4014               ; DMA from $0200-$02FF

        ; --- set PPU address ---
        bit $2002               ; reset $2005/$2006 write latch
        lda $01
        sta $2006               ; high byte
        lda $00
        sta $2006               ; low byte (increment is +1 from PPUCTRL=$80)

        ; --- fully unrolled copy: 8 cycles per byte ---
        .repeat 160, I
        lda $0300+I
        sta $2007
        .endrep

        ; --- restore PPUCTRL and scroll (writes to $2006 clobbered 't') ---
        lda #$80
        sta $2000               ; NMI on, nametable $2000, inc 1, patterns $0000
        lda #$00
        sta $2005               ; X scroll = 0
        sta $2005               ; Y scroll = 0
        rts
