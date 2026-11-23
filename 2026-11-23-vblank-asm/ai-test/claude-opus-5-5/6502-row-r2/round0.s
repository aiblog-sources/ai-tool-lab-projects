; vblank_update: copy $0300-$031F to row ($00) of nametable 0 ($2000 + row*32)
vblank_update:
        bit $2002               ; reset PPU address latch
        lda $00                 ; row (0-29)
        lsr a                   ; high byte = $20 + (row >> 3)
        lsr a
        lsr a
        ora #$20
        sta $2006
        lda $00                 ; low byte = (row & 7) << 5
        asl a
        asl a
        asl a
        asl a
        asl a
        sta $2006
        ldx #0
@loop:
        lda $0300,x
        sta $2007
        inx
        cpx #32
        bne @loop
        lda #$80                ; NMI on, nametable $2000, increment 1
        sta $2000
        lda #0                  ; scroll X=0, Y=0
        sta $2005
        sta $2005
        rts
