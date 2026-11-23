```asm
; vblank_update: OAM DMA from page $02, then copy $0300-$039F (160 bytes)
; to VRAM starting at the address in ($00 low, $01 high), then restore
; PPUCTRL=$80 and scroll to (0,0).
; Clobbers A. Writes no RAM.

vblank_update:
        ; --- OAM DMA ---
        lda #$00
        sta $2003               ; OAMADDR = 0
        lda #$02
        sta $4014               ; DMA from $0200-$02FF (513/514 cycles)

        ; --- Set VRAM address ---
        bit $2002               ; reset the $2005/$2006 write latch
        lda $01
        sta $2006               ; high byte
        lda $00
        sta $2006               ; low byte
                                ; PPUCTRL is $80 here, so VRAM increment is +1

        ; --- Copy 160 bytes, fully unrolled (8 cycles per byte) ---
.repeat 160, I
        lda $0300 + I
        sta $2007
.endrepeat

        ; --- Restore PPUCTRL and scroll ---
        lda #$80
        sta $2000               ; NMI on, nametable $2000, inc 1 (fixes t's NT bits)
        lda #$00
        sta $2005               ; X scroll = 0
        sta $2005               ; Y scroll = 0
        rts
```

VBlank budget: 2273
Cycles used: 1846