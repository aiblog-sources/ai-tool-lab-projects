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
