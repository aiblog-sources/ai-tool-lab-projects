```asm
; vblank_update: copy $0300-$031F to row ($00) of nametable 0 ($2000 + row*32)
vblank_update:
        bit $2002               ; reset the $2005/$2006 write latch

        ; High byte = $20 + (row >> 3)
        lda $00
        lsr a
        lsr a
        lsr a
        ora #$20
        sta $2006

        ; Low byte = (row << 5) & $FF
        lda $00
        asl a
        asl a
        asl a
        asl a
        asl a
        sta $2006

        ; Copy 32 bytes (PPUCTRL is $80, so VRAM increment is +1)
        ldx #0
@copy:
        lda $0300,x
        sta $2007
        inx
        cpx #32
        bne @copy

        ; Restore PPUCTRL and the scroll for the next frame
        lda #$80                ; NMI on, nametable $2000, increment 1
        sta $2000
        lda #0
        sta $2005               ; X scroll = 0
        sta $2005               ; Y scroll = 0
        rts
```
VBlank budget: 2273
Cycles used: 539