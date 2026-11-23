The full job can't fit in NTSC vblank. The OAM DMA costs 513-514 cycles, and the fastest unrolled copy is `LDA abs` + `STA $2007` at 8 cycles per byte. 256 bytes then cost 2048 cycles, so the total is about 2560 plus setup, against roughly 2273 cycles of vblank. Rendering must stay on and all PPU work must finish inside vblank, so I made the routine copy only the first 200 bytes. This is a limit of the hardware and the rules, not of the code. I have not assembled or run this. To get all 256 bytes across, split the copy over two frames, which needs RAM for state that the rules don't allow, or turn rendering off.

```asm
VBLANK_UPDATE_COPY_COUNT = 200      ; max that fits in vblank (256 does not)

vblank_update:
        ; --- OAM DMA from $0200 ---
        lda #$00
        sta $2003
        lda #$02
        sta $4014               ; 513/514 cycles

        ; --- set PPU address from $00/$01 ---
        bit $2002               ; reset write latch
        lda $01
        sta $2006
        lda $00
        sta $2006

        ; --- unrolled copy, 8 cycles/byte ---
        .repeat VBLANK_UPDATE_COPY_COUNT, i
        lda $0300+i
        sta $2007
        .endrepeat

        ; --- restore scroll and PPUCTRL ---
        bit $2002               ; reset write latch
        lda #$00
        sta $2005               ; X scroll = 0
        sta $2005               ; Y scroll = 0
        lda #$80
        sta $2000               ; nametable $2000, NMI on, inc 1
        rts
```

VBlank budget: 2273
Cycles used: 2170 (worst case, from the JSR to the final `STA $2000`; copies only 200 of the 256 bytes)