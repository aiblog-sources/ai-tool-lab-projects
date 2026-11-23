The 160-byte copy is fully unrolled so everything fits in NTSC vblank.

- **Timing:** each byte costs `LDA abs` + `STA $2007`, which is 8 cycles. A normal indexed loop costs about 13 cycles per byte and would run out of vblank.
- **Total:** NMI entry and register saves (about 20 cycles), the JSR (6), OAM DMA (about 520), the copy (1280) and the scroll reset add up to about 1860 cycles. NTSC vblank is about 2270 cycles.
- **Scroll reset:** after the copy, the routine resets the scroll. Writes to `$2006` overwrite the PPU's internal `t` register, and that register is reloaded into `v` before the next frame. Without the reset, the screen would scroll to the wrong place.

```asm
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
```