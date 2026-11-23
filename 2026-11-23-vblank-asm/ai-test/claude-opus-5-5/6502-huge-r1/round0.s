; vblank_update -- fastest spec-compliant version.
; WARNING: ~2614 cycles from JSR; exceeds NTSC vblank (~2273 cycles total).
; See notes: requires splitting the copy (or forced-blank extension) to be safe.

vblank_update:
        ; --- OAM DMA from page $02 ---
        lda #$00
        sta $2003               ; OAMADDR = 0
        lda #$02
        sta $4014               ; OAM DMA, 513/514 cycles

        ; --- set VRAM address from ($00/$01) ---
        bit $2002               ; reset $2005/$2006 write latch
        lda $01
        sta $2006               ; high byte
        lda $00
        sta $2006               ; low byte

        ; --- copy $0300-$03FF to VRAM, fully unrolled (8 cycles/byte) ---
        .repeat 256, I
        lda $0300+I
        sta $2007
        .endrepeat

        ; --- restore scroll 0,0 and PPUCTRL $80 (nametable $2000) ---
        lda #$00
        sta $2005               ; X scroll = 0
        sta $2005               ; Y scroll = 0
        lda #$80
        sta $2000               ; NMI on, nametable $2000, inc 1
        rts
