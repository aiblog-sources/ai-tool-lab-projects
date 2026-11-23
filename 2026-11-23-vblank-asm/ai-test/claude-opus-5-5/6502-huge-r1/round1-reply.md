```asm
; vblank_update -- OAM DMA, then half of the 256-byte copy each frame.
;
; Why the copy is split: a full copy cannot fit in one NTSC vertical blank.
;   - NTSC vblank is about 2273 CPU cycles (20 lines * 341 dots / 3).
;   - The harness measured the end of vblank at 2263 cycles after NMI.
;   - OAM DMA takes 513/514 cycles.
;   - The fastest copy is LDA abs + STA $2007, 8 cycles per byte.
;     256 bytes would take 2048 cycles.
;   - DMA plus copy is about 2560 cycles, more than vblank. No code
;     arrangement fixes that.
;
; So the copy alternates between two halves:
;   - even phase: $0300-$037F -> (dest)     .. (dest)+$7F
;   - odd phase:  $0380-$03FF -> (dest)+$80 .. (dest)+$FF
; The phase bit is kept in scratch zero page $10. Its starting value does
; not matter. After any two calls in a row, all 256 bytes are in VRAM.
; The worst-case path ends about 1613 cycles after the JSR, well inside
; vblank.

vblank_update:
        ; --- OAM DMA from page $02 ---
        lda #$00
        sta $2003               ; OAMADDR = 0
        lda #$02
        sta $4014               ; OAM DMA (513/514 cycles)

        bit $2002               ; reset $2005/$2006 write latch

        ; --- flip the phase bit and pick a half ---
        lda $10
        eor #$01
        sta $10
        lsr a                   ; bit 0 -> carry
        bcc @first_half
        jmp @second_half

@first_half:
        lda $01
        sta $2006               ; VRAM address high = dest high
        lda $00
        sta $2006               ; VRAM address low  = dest low
        .repeat 128, I
        lda $0300+I
        sta $2007
        .endrepeat
        jmp @finish

@second_half:
        lda $00
        clc
        adc #$80
        tax                     ; low byte of dest+$80
        lda $01
        adc #$00                ; high byte of dest+$80 (stays below $24)
        sta $2006
        stx $2006
        .repeat 128, I
        lda $0380+I
        sta $2007
        .endrepeat

@finish:
        ; --- scroll 0,0 and PPUCTRL $80 (nametable $2000, inc 1, NMI on) ---
        lda #$00
        sta $2005               ; X scroll = 0
        sta $2005               ; Y scroll = 0
        lda #$80
        sta $2000
        rts
```

VBlank budget: 2273
Cycles used: 1613

The full 256-byte copy plus OAM DMA needs about 2560 cycles, which is more than the vblank budget. So each call writes 128 bytes and switches halves, and the whole destination is correct after two frames in a row. On the very first frame, only one half is written.