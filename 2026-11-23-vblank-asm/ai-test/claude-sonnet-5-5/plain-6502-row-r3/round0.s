; vblank_update: copy 32 bytes from $0300-$031F to the nametable 0 row in $00.
; Clobbers A and X. Uses no RAM.

PPUCTRL   = $2000
PPUSTATUS = $2002
PPUSCROLL = $2005
PPUADDR   = $2006
PPUDATA   = $2007

vblank_update:
        lda PPUSTATUS           ; reset the address latch
        lda $00                 ; row 0-29
        lsr a
        lsr a
        lsr a                   ; row >> 3
        ora #$20                ; high byte = $20 + (row >> 3)
        sta PPUADDR
        lda $00
        asl a
        asl a
        asl a
        asl a
        asl a                   ; low byte = (row & 7) << 5
        sta PPUADDR

        ldx #0
@copy:  lda $0300,x
        sta PPUDATA
        inx
        cpx #32
        bne @copy

        ; Restore the scroll and nametable bits that the $2006 writes clobbered
        lda PPUSTATUS           ; reset the latch
        lda #$80
        sta PPUCTRL             ; NMI on, nametable $2000
        lda #0
        sta PPUSCROLL           ; X scroll = 0
        sta PPUSCROLL           ; Y scroll = 0
        rts
