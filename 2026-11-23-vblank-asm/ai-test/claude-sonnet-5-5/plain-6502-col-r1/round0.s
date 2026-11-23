; vblank_update
; Copies the 30 bytes at $0300-$031D down column ($00) of nametable 0.
; Byte i goes to $2000 + i*32 + column.
; Uses PPUCTRL bit 2 (+32 increment) so one address setup covers the column.
; Afterwards it restores PPUCTRL to $80 and resets the scroll to 0,0.

PPUCTRL   = $2000
PPUSTATUS = $2002
PPUSCROLL = $2005
PPUADDR   = $2006
PPUDATA   = $2007

vblank_update:
        lda PPUSTATUS           ; reset the address/scroll latch
        lda #$84                ; NMI on, VRAM increment +32, nametable $2000
        sta PPUCTRL
        lda #$20
        sta PPUADDR             ; high byte: $20
        lda $00                 ; column 0..31
        sta PPUADDR             ; low byte: column (row 0)
        ldx #0
@loop:
        lda $0300,x
        sta PPUDATA
        inx
        cpx #30
        bne @loop

        lda PPUSTATUS           ; reset the latch again
        lda #$80                ; restore NMI on, increment +1
        sta PPUCTRL
        lda #$00
        sta PPUSCROLL           ; scroll X = 0
        sta PPUSCROLL           ; scroll Y = 0
        rts
