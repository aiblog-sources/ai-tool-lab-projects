Copying all 256 bytes in one vblank is not possible, so this version copies 128 bytes per frame. The full 256-byte update now takes two frames.

**Why one frame can't work.** Your test shows vblank ending 2261 cycles after the NMI handler starts. That leaves about 2250 cycles from the JSR.
- The OAM DMA alone costs 513 or 514 cycles, plus 6 to start it.
- Each byte needs a load from $0300-$03FF and a `STA $2007`. The fastest way is `LDA abs` or `PLA` (4 cycles) plus the store (4 cycles), so 8 cycles per byte. 256 bytes take 2048 cycles.
- Together that is about 2580 cycles, which is more than 2250. No unrolling or trick fixes this when the data can be any value.

**What the routine does instead.**
- It does the OAM DMA every frame.
- It alternates which half it copies: $0300-$037F to dest+0, or $0380-$03FF to dest+$80.
- It stores the frame parity in zero page `$10`, which is allowed scratch space. This only works if nothing else changes `$10` between NMIs.
- The worst case is about 1605 cycles, well inside vblank.

**Two side effects you should know about:**
- If the data at $0300 or the address at $00/$01 changes every frame, the two halves on screen can come from different frames.
- A test that checks all 256 destination bytes after every frame may still report errors on half of them.

If you need all 256 bytes in one frame, the only option is forced blanking: turn rendering off for a few scanlines at the top of the screen. That breaks your "finish inside vblank" rule, so I didn't do it.

```asm
; vblank_update -- OAM DMA every frame, then copy HALF of $0300-$03FF
; to VRAM per frame (alternating halves), then PPUCTRL=$80, scroll 0,0.
; A full 256-byte copy + OAM DMA (~2580 cycles) cannot fit in NTSC vblank
; (~2250 cycles available after NMI entry/JSR), so the copy is split
; across two consecutive frames. Parity flag kept in zero page $10.
vblank_update:
        lda #$02                ; 2
        sta $4014               ; 4 + 513/514  OAM DMA from $0200

        bit $2002               ; 4  reset $2005/$2006 write latch

        lda $10                 ; 3
        eor #$01                ; 2
        sta $10                 ; 3  toggle half selector
        beq @half0              ; 2/3
        jmp @half1              ; 3

; ---- half 0: $0300-$037F -> dest+0 ----
@half0:
        lda $01                 ; 3
        sta $2006               ; 4
        lda $00                 ; 3
        sta $2006               ; 4

        .repeat 128, I
        lda $0300+I             ; 4
        sta $2007               ; 4
        .endrepeat

        lda #$80                ; 2
        sta $2000               ; 4  NMI on, nametable $2000, inc 1
        lda #$00                ; 2
        sta $2005               ; 4  scroll X = 0
        sta $2005               ; 4  scroll Y = 0
        rts

; ---- half 1: $0380-$03FF -> dest+$80 ----
@half1:
        lda $00                 ; 3
        clc                     ; 2
        adc #$80                ; 2
        tax                     ; 2  low byte of dest+$80
        lda $01                 ; 3
        adc #$00                ; 2  high byte (+carry)
        sta $2006               ; 4
        stx $2006               ; 4

        .repeat 128, I
        lda $0380+I             ; 4
        sta $2007               ; 4
        .endrepeat

        lda #$80                ; 2
        sta $2000               ; 4  NMI on, nametable $2000, inc 1
        lda #$00                ; 2
        sta $2005               ; 4  scroll X = 0
        sta $2005               ; 4  scroll Y = 0
        rts
```

VBlank budget: 2273
Cycles used: 1605