; vblank_update - called once per frame from the NMI handler (NTSC).
;   1) OAM DMA from page $02
;   2) Copy $0300-$039F (160 bytes) to VRAM at the address in ($00 low, $01 high)
;   3) Restore PPUCTRL = $80 and scroll = (0,0)
; The copy is fully unrolled (8 cycles per byte) so everything fits in vblank.

vblank_update:
        lda #$00                ; 2
        sta $2003               ; 4  OAMADDR = 0
        lda #$02                ; 2
        sta $4014               ; 4  + 513/514 cycles of OAM DMA

        bit $2002               ; 4  reset the $2005/$2006 write latch
        lda $01                 ; 3
        sta $2006               ; 4  VRAM address high byte
        lda $00                 ; 3
        sta $2006               ; 4  VRAM address low byte

        ; PPUCTRL is still $80, so VRAM increment is 1
        .repeat 160, i
        lda $0300 + i           ; 4
        sta $2007               ; 4
        .endrepeat

        ; Writing $2006 changed the PPU's internal "t" register,
        ; so set the nametable and scroll again.
        lda #$80                ; 2
        sta $2000               ; 4  NMI on, nametable $2000, inc 1
        lda #$00                ; 2
        sta $2005               ; 4  X scroll = 0
        sta $2005               ; 4  Y scroll = 0
        rts                     ; 6
