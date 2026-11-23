; vblank_update
; Copies the 30 bytes at $0300-$031D down column ($00) of nametable 0.
; Byte i goes to PPU $2000 + i*32 + column.
; Called from NMI with JSR. Changes A and X.

vblank_update:
        bit $2002               ; reset the $2005/$2006 write latch

        lda #$84                ; NMI on, VRAM increment 32 (bit 2)
        sta $2000

        lda #$20                ; PPU address = $2000 + column
        sta $2006
        lda $00                 ; column 0..31 (no carry into high byte)
        sta $2006

        ldx #0
@loop:
        lda $0300,x
        sta $2007
        inx
        cpx #30
        bne @loop

        lda #$80                ; back to NMI on, nametable $2000, increment 1
        sta $2000

        lda #0                  ; scroll X = 0, Y = 0
        sta $2005
        sta $2005

        rts
