```asm
; vblank_update
; Copies 30 bytes from $0300-$031D down nametable 0 column ($00), using
; the PPU's +32 VRAM increment mode. Then restores PPUCTRL and the scroll.
; Takes about 520 CPU cycles, well inside NTSC vblank.

vblank_update:
        lda #$84                ; NMI on, VRAM increment 32, NT $2000
        sta $2000
        bit $2002               ; reset the $2005/$2006 write latch

        lda #$20                ; high byte of $2000 + column
        sta $2006
        lda $00
        and #$1F                ; keep column in 0..31
        sta $2006               ; low byte = column (row 0)

        ldx #0
@loop:
        lda $0300,x
        sta $2007               ; VRAM address advances by 32 (next row)
        inx
        cpx #30
        bne @loop

        ; Restore PPUCTRL (increment 1, base nametable $2000) and
        ; reset scroll to 0,0 so the $2006 writes don't disturb rendering.
        lda #$80
        sta $2000
        lda #$00
        sta $2005               ; X scroll = 0
        sta $2005               ; Y scroll = 0
        rts
```