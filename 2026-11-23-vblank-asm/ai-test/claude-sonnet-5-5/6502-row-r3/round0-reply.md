```asm
vblank_update:
        lda $00                 ; row 0-29
        lsr a
        lsr a
        lsr a                   ; row >> 3 (0-3)
        ora #$20                ; high byte of $2000 + row*32
        tax
        bit $2002               ; reset PPU address latch
        stx $2006               ; high byte
        lda $00
        asl a
        asl a
        asl a
        asl a
        asl a                   ; (row & 7) << 5, upper bits shift out
        and #$E0
        sta $2006               ; low byte
        ldx #0
@copy:
        lda $0300,x
        sta $2007
        inx
        cpx #32
        bne @copy
        lda #0                  ; restore scroll X=0, Y=0
        sta $2005
        sta $2005
        lda #$80                ; PPUCTRL back to $80 (also clears nametable bits in t)
        sta $2000
        rts
```

VBlank budget: 2273
Cycles used: 543