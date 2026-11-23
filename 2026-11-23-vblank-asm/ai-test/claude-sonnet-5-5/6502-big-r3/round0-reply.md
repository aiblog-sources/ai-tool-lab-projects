```asm
; vblank_update
; Called by JSR from the NMI handler, rendering on.
; 1. OAM DMA from $0200.
; 2. Copy 160 bytes from $0300-$039F to PPU address ($01:$00).
; 3. Restore PPUCTRL=$80 and scroll 0,0.
; Clobbers A only.

vblank_update:
        lda #$02
        sta $4014               ; OAM DMA (513/514 cycles extra)

        bit $2002               ; reset the $2005/$2006 write latch
        lda $01
        sta $2006
        lda $00
        sta $2006

        ; Fully unrolled copy: LDA abs / STA $2007 = 8 cycles per byte
        .repeat 160, i
        lda $0300+i
        sta $2007
        .endrepeat

        ; Restore nametable $2000 and scroll 0,0 (overwrites t after the $2006 writes)
        lda #$80
        sta $2000
        lda #$00
        sta $2005               ; X scroll = 0 (latch was reset by the $2006 pair)
        sta $2005               ; Y scroll = 0
        rts
```

VBlank budget: 2273
Cycles used: 1840

The 1840 breaks down as JSR 6, DMA setup and stall 520 (assuming the worst case of 514 for the stall), PPU address setup 18, the 160-byte copy 1280, and the final PPUCTRL and scroll writes 16. The count does not include the NMI entry overhead from the handler's register saves, which leaves about 430 cycles of slack.