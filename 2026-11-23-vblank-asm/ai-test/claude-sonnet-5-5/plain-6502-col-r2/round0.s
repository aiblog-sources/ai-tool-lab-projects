vblank_update:
        lda $2002               ; reset PPU address latch
        lda #$84                ; NMI on, VRAM increment 32 (down)
        sta $2000
        lda #$20
        sta $2006
        lda $00                 ; column 0-31 -> low address byte
        and #$1F
        sta $2006
        ldx #0
@loop:  lda $0300,x
        sta $2007
        inx
        cpx #30
        bne @loop
        lda #$80                ; restore increment 1, NMI on, nametable $2000
        sta $2000
        lda #0                  ; reset scroll to 0,0 (PPUADDR writes clobbered it)
        sta $2005
        sta $2005
        rts
