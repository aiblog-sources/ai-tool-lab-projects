The routine switches the PPU to increment-by-32 mode, so each write to $2007 moves down one row. It writes the 30 bytes, then puts the PPU registers back.

```asm
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
```

Notes:
- **Timing:** the copy loop takes about 450 cycles, well within the NTSC vblank window of about 2270 cycles.
- **NMI:** bit 7 of PPUCTRL stays set the whole time. Because NMI is never switched from off to on, the $2000 writes cannot trigger a second NMI.
- **Scroll:** the routine rewrites $2000 with $80 and writes 0 twice to $2005. That resets the PPU's internal scroll and nametable state that the $2006 writes changed, so the next frame starts rendering at $2000 with no scroll.
- **Memory:** it only uses A, X and the PPU registers. It writes no RAM and uses no stack.