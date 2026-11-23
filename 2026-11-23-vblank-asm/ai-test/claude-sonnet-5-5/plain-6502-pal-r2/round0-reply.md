```asm
vblank_update:
        lda $2002               ; reset the PPU address/scroll latch
        lda #$3F
        sta $2006               ; PPU address high = $3F
        lda #$00
        sta $2006               ; PPU address low  = $00 -> $3F00
        tax
@copy:
        lda $0300,x
        sta $2007               ; VRAM increment is 1, so this walks $3F00-$3F1F
        inx
        cpx #$20
        bne @copy

        ; Restore the scroll/address so rendering resumes at $2000, no scroll.
        ; Without this, v is left at $3F20 and the picture would be corrupted.
        lda #$00
        sta $2006
        sta $2006               ; t = v = $0000
        sta $2005
        sta $2005               ; fine X = 0, scroll X/Y = 0
        rts
```

The routine reads $2002 to reset the write latch, then sets the PPU address to $3F00 and copies the 32 bytes from $0300-$031F to $2007. The loop takes well under the NTSC vblank budget. Finally it writes $0000 to $2006 and zeros to $2005, so the PPU's internal address registers point at nametable $2000 with no scroll when rendering resumes.