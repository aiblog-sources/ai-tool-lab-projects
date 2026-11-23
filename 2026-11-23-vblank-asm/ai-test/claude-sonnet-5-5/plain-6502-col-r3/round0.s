PPUCTRL   = $2000
PPUSCROLL = $2005
PPUADDR   = $2006
PPUDATA   = $2007
PPUSTATUS = $2002

vblank_update:
        lda PPUSTATUS           ; reset the address/scroll write latch
        lda #$84                ; NMI on, VRAM increment 32 (down a column)
        sta PPUCTRL
        lda #$20                ; high byte of $2000 + column
        sta PPUADDR
        lda $00                 ; column 0-31 is the low byte
        sta PPUADDR
        ldx #0
@loop:  lda $0300,x
        sta PPUDATA             ; PPU address advances by 32 after each write
        inx
        cpx #30
        bne @loop
        lda #$80                ; restore increment 1, base nametable $2000
        sta PPUCTRL
        lda #0                  ; reset scroll to 0,0 after the $2006 writes
        sta PPUSCROLL
        sta PPUSCROLL
        rts
