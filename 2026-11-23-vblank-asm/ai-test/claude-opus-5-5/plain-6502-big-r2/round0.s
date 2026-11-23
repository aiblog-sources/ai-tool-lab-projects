; vblank_update
; Called from NMI (NTSC, rendering on). Uses A only (X/Y untouched),
; no scratch RAM needed.
;   1. OAM DMA from page $02
;   2. Copy $0300-$039F (160 bytes) to PPU address in ($00/$01)
;   3. Restore PPUCTRL / scroll so nametable $2000 is shown at (0,0)

vblank_update:
        ; ---- OAM DMA ----
        lda #$00
        sta $2003               ; OAMADDR = 0
        lda #$02
        sta $4014               ; DMA from $0200-$02FF (513/514 cycles)

        ; ---- set PPU address ----
        lda $2002               ; reset $2005/$2006 write latch
        lda $01
        sta $2006               ; high byte
        lda $00
        sta $2006               ; low byte

        ; ---- copy 160 bytes, fully unrolled (8 cycles/byte) ----
        .repeat 160, I
        lda $0300+I
        sta $2007
        .endrepeat

        ; ---- restore scroll / nametable (t register) ----
        lda #$80
        sta $2000               ; NMI on, nametable $2000, inc 1, pat $0000
        lda #$00
        sta $2005               ; X scroll = 0
        sta $2005               ; Y scroll = 0
        rts
