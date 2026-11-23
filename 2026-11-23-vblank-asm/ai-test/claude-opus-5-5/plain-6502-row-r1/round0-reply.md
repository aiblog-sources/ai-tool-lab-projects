```asm
; vblank_update
; Copies the 32 bytes at $0300-$031F to row ($00) of nametable 0
; (PPU address $2000 + row*32), then restores the scroll.
; Uses A and X. Writes no RAM.

vblank_update:
        lda $2002               ; reset the PPU address latch

        ; high byte = $20 | (row >> 3)
        lda $00
        lsr a
        lsr a
        lsr a
        ora #$20
        sta $2006

        ; low byte = (row << 5) & $FF
        lda $00
        asl a
        asl a
        asl a
        asl a
        asl a
        sta $2006

        ; copy 32 bytes (PPUCTRL is $80, so VRAM increment is 1)
        ldx #0
@loop:
        lda $0300,x
        sta $2007
        inx
        cpx #32
        bne @loop

        ; restore scroll: nametable $2000, X = 0, Y = 0
        lda #$80
        sta $2000
        lda #0
        sta $2005
        sta $2005
        rts
```