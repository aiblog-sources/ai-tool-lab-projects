```asm
; vblank_update: copy $0300-$031F to palette $3F00-$3F1F, then reset scroll.
; Called via JSR from the NMI handler. Clobbers A. Uses no RAM.

vblank_update:
        lda $2002               ; 4  reset $2005/$2006 write latch (also clears VBL flag)
        lda #$3F                ; 2
        sta $2006               ; 4  PPU address high = $3F
        lda #$00                ; 2
        sta $2006               ; 4  PPU address low  = $00 (VRAM increment is 1)

        ; Fully unrolled copy: 32 x (4 + 4) = 256 cycles
        .repeat 32, I
        lda $0300+I             ; 4
        sta $2007               ; 4
        .endrepeat

        lda #$80                ; 2
        sta $2000               ; 4  NMI on, nametable $2000, inc 1, patterns $0000
        lda #$00                ; 2
        sta $2005               ; 4  scroll X = 0
        sta $2005               ; 4  scroll Y = 0
        rts                     ; 6
```

VBlank budget: 2273
Cycles used: 294