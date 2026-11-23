```asm
; vblank_update
; Copies $0300-$031F to PPU palette $3F00-$3F1F during vblank, then
; restores the scroll so that nametable $2000 shows at (0,0).
; Cost: about 540 CPU cycles, which fits well inside NTSC vblank
; (about 2270 cycles).

vblank_update:
        bit $2002               ; reset the $2005/$2006 write latch
        lda #$3F
        sta $2006               ; PPU address high byte = $3F
        ldx #$00
        stx $2006               ; PPU address low byte = $00 -> $3F00

@loop:
        lda $0300,x
        sta $2007               ; VRAM increment is 1 (PPUCTRL bit 2 = 0)
        inx
        cpx #$20
        bne @loop

        ; Move the VRAM address away from the palette.
        lda #$00
        sta $2006
        sta $2006

        ; Restore the scroll. Writing $2006 overwrote the PPU's internal
        ; t/v registers, so set them again: nametable $2000, X=0, Y=0.
        bit $2002               ; reset the write latch
        lda #$80
        sta $2000               ; NMI on, nametable $2000, increment 1, patterns $0000
        lda #$00
        sta $2005               ; X scroll = 0
        sta $2005               ; Y scroll = 0
        rts
```