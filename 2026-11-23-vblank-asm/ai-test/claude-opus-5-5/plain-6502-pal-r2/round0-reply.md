```asm
; vblank_update
; Copies $0300-$031F to palette RAM $3F00-$3F1F, then restores the
; scroll and base nametable so rendering starts at $2000, X=0, Y=0.
; Uses A and X only. Writes no RAM.
; The copy takes about 600 cycles, well within NTSC vblank (~2270 cycles).

vblank_update:
        lda #$80                ; NMI on, nametable $2000, VRAM increment +1
        sta $2000

        bit $2002               ; reset the $2005/$2006 write latch

        lda #$3F                ; PPU address = $3F00
        sta $2006
        lda #$00
        sta $2006

        ldx #$00
@loop:
        lda $0300,x
        sta $2007
        inx
        cpx #$20
        bne @loop

        ; Writing $2006 changed the PPU's internal v/t registers,
        ; so the scroll must be set again before rendering starts.
        bit $2002               ; reset the write latch
        lda #$80                ; nametable select bits = 0 (nametable $2000)
        sta $2000
        lda #$00
        sta $2005               ; X scroll = 0
        sta $2005               ; Y scroll = 0

        rts
```