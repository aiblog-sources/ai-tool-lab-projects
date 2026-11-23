; vblank_update
; Copies 32 bytes from $0300-$031F to row ($00) of nametable 0
; at PPU address $2000 + row*32, then restores PPUCTRL and scroll.
; Uses A and X. Writes no RAM.

vblank_update:
        bit $2002               ; reset the PPU address latch

        ; High byte of address = $20 + (row >> 3)
        lda $00
        lsr a
        lsr a
        lsr a
        ora #$20
        sta $2006

        ; Low byte of address = (row << 5) & $FF
        lda $00
        asl a
        asl a
        asl a
        asl a
        asl a
        sta $2006

        ; Copy 32 bytes (PPUCTRL increment is +1)
        ldx #$00
@loop:
        lda $0300,x
        sta $2007
        inx
        cpx #$20
        bne @loop

        ; Restore PPUCTRL (nametable $2000, inc 1, NMI on). This also
        ; puts the nametable select bits back after the $2006 writes changed them.
        lda #$80
        sta $2000

        ; Reset scroll to 0,0 (the latch is back at the first write
        ; after the two $2006 writes)
        lda #$00
        sta $2005
        sta $2005

        rts
