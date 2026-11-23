The routine sets the PPU to step 32 bytes per write, so each write to `$2007` moves one row down the column. It then copies the 30 bytes with an unrolled loop. Afterwards it puts PPUCTRL and the scroll back as they were.

```asm
; vblank_update
; Copies the 30 bytes at $0300-$031D into one column of nametable 0.
; The column number (0-31) is in zero page $00.
; Byte i goes to PPU address $2000 + i*32 + column.
; Uses the VRAM increment-32 mode, so each $2007 write moves down one row.
; Changes A only. No RAM is written.

vblank_update:
        bit $2002               ; reset the $2005/$2006 write latch
        lda #$84                ; NMI on, VRAM increment 32, nametable $2000
        sta $2000

        lda #$20                ; high byte of the address: $20
        sta $2006
        lda $00
        and #$1F                ; keep the column in 0-31
        sta $2006               ; low byte of the address: $00 + column

        ; Unrolled copy: 30 x (LDA abs + STA abs) = 240 cycles
        .repeat 30, I
        lda $0300 + I
        sta $2007
        .endrepeat

        ; Put back PPUCTRL: base nametable $2000, increment 1, NMI on
        lda #$80
        sta $2000

        ; Writing $2006 changed the scroll, so reset it to 0,0
        lda #$00
        sta $2005
        sta $2005
        rts
```