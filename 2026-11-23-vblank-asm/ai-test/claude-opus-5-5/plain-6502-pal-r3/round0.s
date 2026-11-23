; vblank_update
; Copies 32 bytes from $0300-$031F to PPU palette $3F00-$3F1F,
; then restores PPUCTRL and the scroll so rendering continues
; from nametable $2000 at (0,0).
;
; Timing: about 500 CPU cycles, well inside the NTSC vblank
; (about 2270 cycles).

vblank_update:
        bit $2002               ; reset the PPU address/scroll latch

        lda #$80                ; NMI on, nametable $2000, VRAM increment +1
        sta $2000

        lda #$3F                ; PPU address = $3F00
        sta $2006
        lda #$00
        sta $2006

        ldx #$00
@loop:
        lda $0300,x
        sta $2007
        inx
        cpx #$20
        bne @loop

        ; Restore the rendering state. Writing PPUADDR changed the
        ; internal 't' register, so set the nametable bits and the
        ; scroll again.
        lda #$80
        sta $2000               ; base nametable $2000
        bit $2002               ; reset the latch before the scroll writes
        lda #$00
        sta $2005               ; X scroll = 0
        sta $2005               ; Y scroll = 0

        rts
