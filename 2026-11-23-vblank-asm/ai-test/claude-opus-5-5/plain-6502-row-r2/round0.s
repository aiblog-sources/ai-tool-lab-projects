; vblank_update
; Copies 32 bytes from $0300-$031F to row ($00) of nametable 0 ($2000 + row*32),
; then restores PPUCTRL and the scroll so the display is unaffected.

vblank_update:
        bit $2002               ; reset the $2005/$2006 write latch

        ; High byte of address = $20 + (row >> 3)
        lda $00
        lsr a
        lsr a
        lsr a
        ora #$20
        sta $2006

        ; Low byte of address = (row & 7) << 5
        lda $00
        asl a
        asl a
        asl a
        asl a
        asl a
        sta $2006

        ; Copy 32 bytes, VRAM increment is 1 (PPUCTRL bit 2 = 0)
        ldx #0
@loop:
        lda $0300,x
        sta $2007
        inx
        cpx #32
        bne @loop

        ; Restore PPUCTRL (NMI on, nametable $2000, inc 1) and the scroll
        lda #$80
        sta $2000
        bit $2002               ; make sure the latch is reset
        lda #0
        sta $2005               ; X scroll = 0
        sta $2005               ; Y scroll = 0
        rts
