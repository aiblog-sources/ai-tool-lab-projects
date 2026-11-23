```asm
; vblank_update: copy 30 bytes at $0300-$031D down one column of nametable 0.
; Column number is in zero page $00 (0-31).
; Uses PPUCTRL bit 2 (increment 32) so one PPUADDR setup covers the whole column.

PPUCTRL   = $2000
PPUSCROLL = $2005
PPUADDR   = $2006
PPUDATA   = $2007
PPUSTATUS = $2002

vblank_update:
        lda #$84            ; NMI on, increment 32, NT $2000, patterns at $0000
        sta PPUCTRL
        lda PPUSTATUS       ; reset the address latch
        lda #$20
        sta PPUADDR         ; high byte of $2000 + column
        lda $00
        sta PPUADDR         ; low byte = column (row 0)
        ldx #0
@loop:
        lda $0300,x
        sta PPUDATA         ; address advances by 32 (next row)
        inx
        cpx #30
        bne @loop

        lda #$80            ; restore PPUCTRL: increment 1, nametable $2000
        sta PPUCTRL
        lda #0
        sta PPUSCROLL       ; X = 0
        sta PPUSCROLL       ; Y = 0
        rts
```

VBlank budget: 2273
Cycles used: 496