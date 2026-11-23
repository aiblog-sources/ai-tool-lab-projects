; vblank_update - copy $0300-$031F to palette RAM $3F00-$3F1F
; Called via JSR from NMI (NTSC). Clobbers A only.
; The palette copy is fully unrolled to keep vblank time low.

vblank_update:
        bit $2002               ; 4  reset the PPU address/scroll write latch
        lda #$3F                ; 2
        sta $2006               ; 4  PPUADDR high byte
        lda #$00                ; 2
        sta $2006               ; 4  PPUADDR low byte -> $3F00

        .repeat 32, I
        lda $0300+I             ; 4
        sta $2007               ; 4  write palette byte, VRAM address +1
        .endrepeat              ; 32 * 8 = 256

        ; Restore base nametable $2000, increment 1, NMI on
        lda #$80                ; 2
        sta $2000               ; 4  PPUCTRL
        ; Scroll X = 0, Y = 0 (the latch is back on the first write after
        ; the even number of $2006 writes above)
        lda #$00                ; 2
        sta $2005               ; 4  X scroll
        sta $2005               ; 4  Y scroll
        rts
