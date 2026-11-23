; vblank_update: copies $0300-$031F to palette $3F00-$3F1F,
; then sets PPUCTRL to $80 and the scroll to 0,0.
; Called from NMI with rendering on. PPUCTRL is $80, so VRAM increment is +1.

vblank_update:
        lda $2002               ; 4  reset the $2005/$2006 write latch
        lda #$3F                ; 2
        sta $2006               ; 4  PPUADDR high = $3F
        lda #$00                ; 2
        sta $2006               ; 4  PPUADDR low  = $00
        ldx #$00                ; 2
@loop:
        lda $0300,x             ; 4  (no page crossing)
        sta $2007               ; 4
        inx                     ; 2
        cpx #$20                ; 2
        bne @loop               ; 3 taken / 2 last
                                ; loop total: 32*15 - 1 = 479

        ; Set t (the address the PPU reloads from at the pre-render line)
        ; to nametable $2000, scroll 0,0.
        ; The $2006 writes changed t, so $2000 and $2005 must both be rewritten.
        lda #$80                ; 2
        sta $2000               ; 4  NMI on, nametable $2000, inc 1
        lda #$00                ; 2
        sta $2005               ; 4  X scroll = 0
        sta $2005               ; 4  Y scroll = 0 (last PPU write)
        rts
