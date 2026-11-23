PPUCTRL   = $2000
PPUMASK   = $2001
PPUSTATUS = $2002
PPUSCROLL = $2005
PPUADDR   = $2006
PPUDATA   = $2007
OAMDMA    = $4014

vblank_update:
        lda #$00
        sta PPUMASK             ; forced blank: PPU access is now safe
        lda PPUSTATUS           ; reset the $2005/$2006 write latch

        lda #$02
        sta OAMDMA              ; OAM DMA from $0200 (513/514 cycles)

        lda $01
        sta PPUADDR
        lda $00
        sta PPUADDR             ; destination = ($01:$00)

        ; 8 cycles per byte: LDA abs (4) + STA abs (4)
        .repeat 256, i
        lda $0300+i
        sta PPUDATA
        .endrepeat

        ; Restore: nametable $2000, v = t = 0, fine X = 0, scroll 0,0
        lda #$80
        sta PPUCTRL
        lda #$00
        sta PPUADDR
        sta PPUADDR             ; t = v = $0000
        sta PPUSCROLL
        sta PPUSCROLL           ; X = 0, Y = 0
        lda #$1E
        sta PPUMASK             ; rendering back on
        rts
