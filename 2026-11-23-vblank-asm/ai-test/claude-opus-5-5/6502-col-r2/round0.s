; vblank_update
; Copies 30 bytes from $0300-$031D down one column of nametable 0.
; The column number (0-31) is in zero page $00.
; Byte i goes to PPU address $2000 + i*32 + column.
; It writes no RAM, uses no zero-page scratch and does not touch the stack
; except for JSR/RTS.

vblank_update:
        lda $2002               ; 4  reset the $2005/$2006 write latch
        lda #$84                ; 2  NMI on, VRAM increment 32 (moves down a column)
        sta $2000               ; 4
        lda #$20                ; 2  high byte: nametable 0, row 0
        sta $2006               ; 4
        lda $00                 ; 3  low byte = column (0-31)
        sta $2006               ; 4
        ldx #0                  ; 2
@loop:
        lda $0300,x             ; 4  no page crossing (X < 30)
        sta $2007               ; 4  write, then VRAM address += 32
        inx                     ; 2
        cpx #30                 ; 2
        bne @loop               ; 3 if taken, 2 on the last pass
        lda #$80                ; 2  restore PPUCTRL: NMI on, nametable $2000, inc 1
        sta $2000               ; 4
        lda #0                  ; 2
        sta $2005               ; 4  scroll X = 0
        sta $2005               ; 4  scroll Y = 0 (the latch was even after the two $2006 writes)
        rts
