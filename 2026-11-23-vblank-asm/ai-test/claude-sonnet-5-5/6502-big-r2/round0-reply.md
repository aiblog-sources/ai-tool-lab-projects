```asm
vblank_update:
        lda #$02
        sta $4014               ; OAM DMA from $0200 (513/514 cycles stall)

        bit $2002               ; reset PPU address latch
        lda $01
        sta $2006
        lda $00
        sta $2006

        ; 160 bytes, fully unrolled: 8 cycles per byte
        .repeat 160, I
        lda $0300+I
        sta $2007
        .endrepeat

        ; restore scroll = (0,0) and PPUCTRL = $80
        bit $2002               ; reset write latch
        lda #$00
        sta $2005               ; X scroll = 0
        sta $2005               ; Y scroll = 0
        lda #$80
        sta $2000               ; NMI on, nametable $2000, increment 1
        rts
```

VBlank budget: 2273
Cycles used: 1844

The budget is 20 scanlines × 341 dots ÷ 3 ≈ 2273 CPU cycles. The count assumes the worst-case 514-cycle DMA. It breaks down as 12 for JSR plus the DMA setup, 514 for the DMA stall, 18 for the latch reset and PPUADDR setup, 1280 for the unrolled copy, and 20 for the scroll and PPUCTRL restore. That leaves about 430 cycles of margin for NMI entry (7 cycles) and the handler's register saves.